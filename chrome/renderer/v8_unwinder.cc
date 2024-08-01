// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/renderer/v8_unwinder.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "build/build_config.h"
#include "v8/include/v8-isolate.h"

#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
// V8 requires the embedder to establish the architecture define.
#define V8_TARGET_ARCH_ARM 1
#include "v8/include/v8-unwinder-state.h"
#endif

namespace {

class V8Module : public base::ModuleCache::Module {
 public:
  enum CodeRangeType { kEmbedded, kNonEmbedded };

  V8Module(const v8::MemoryRange& memory_range, CodeRangeType code_range_type)
      : memory_range_(memory_range), code_range_type_(code_range_type) {}

  V8Module(const V8Module&) = delete;
  V8Module& operator=(const V8Module&) = delete;

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override {
    return reinterpret_cast<uintptr_t>(memory_range_.start);
  }

  std::string GetId() const override {
    return code_range_type_ == kEmbedded
               ? V8Unwinder::kV8EmbeddedCodeRangeBuildId
               : V8Unwinder::kV8CodeRangeBuildId;
  }

  base::FilePath GetDebugBasename() const override {
    return base::FilePath().AppendASCII(code_range_type_ == kEmbedded
                                            ? "V8 Embedded Code Range"
                                            : "V8 Code Range");
  }

  size_t GetSize() const override { return memory_range_.length_in_bytes; }

  bool IsNative() const override { return false; }

 private:
  const v8::MemoryRange memory_range_;
  const CodeRangeType code_range_type_;
};

// Heterogeneous comparator for MemoryRanges and Modules. Compares on both
// base address and size because the module sizes can be updated while the
// base address remains the same.
struct MemoryRangeModuleCompare {
  bool operator()(const v8::MemoryRange& range,
                  const base::ModuleCache::Module* module) const {
    return std::make_pair(reinterpret_cast<uintptr_t>(range.start),
                          range.length_in_bytes) <
           std::make_pair(module->GetBaseAddress(), module->GetSize());
  }

  bool operator()(const base::ModuleCache::Module* module,
                  const v8::MemoryRange& range) const {
    return std::make_pair(module->GetBaseAddress(), module->GetSize()) <
           std::make_pair(reinterpret_cast<uintptr_t>(range.start),
                          range.length_in_bytes);
  }

  bool operator()(const v8::MemoryRange& a, const v8::MemoryRange& b) const {
    return std::make_pair(a.start, a.length_in_bytes) <
           std::make_pair(b.start, b.length_in_bytes);
  }
};

v8::MemoryRange GetEmbeddedCodeRange(v8::Isolate* isolate) {
  v8::MemoryRange range;
  isolate->GetEmbeddedCodeRange(&range.start, &range.length_in_bytes);
  return range;
}

void CopyCalleeSavedRegisterFromRegisterContext(
    const base::RegisterContext& register_context,
    v8::CalleeSavedRegisters* callee_saved_registers) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  // ARM requires callee-saved registers to be restored:
  // https://crbug.com/v8/10799.
  DCHECK(callee_saved_registers);
  callee_saved_registers->arm_r4 =
      reinterpret_cast<void*>(register_context.arm_r4);
  callee_saved_registers->arm_r5 =
      reinterpret_cast<void*>(register_context.arm_r5);
  callee_saved_registers->arm_r6 =
      reinterpret_cast<void*>(register_context.arm_r6);
  callee_saved_registers->arm_r7 =
      reinterpret_cast<void*>(register_context.arm_r7);
  callee_saved_registers->arm_r8 =
      reinterpret_cast<void*>(register_context.arm_r8);
  callee_saved_registers->arm_r9 =
      reinterpret_cast<void*>(register_context.arm_r9);
  callee_saved_registers->arm_r10 =
      reinterpret_cast<void*>(register_context.arm_r10);
#endif
}

void CopyCalleeSavedRegisterToRegisterContext(
    const v8::CalleeSavedRegisters* callee_saved_registers,
    base::RegisterContext& register_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  DCHECK(callee_saved_registers);
  register_context.arm_r4 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r4);
  register_context.arm_r5 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r5);
  register_context.arm_r6 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r6);
  register_context.arm_r7 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r7);
  register_context.arm_r8 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r8);
  register_context.arm_r9 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r9);
  register_context.arm_r10 =
      reinterpret_cast<uintptr_t>(callee_saved_registers->arm_r10);
#endif
}

}  // namespace

V8Unwinder::V8Unwinder(v8::Isolate* isolate)
    : isolate_(isolate),
      js_entry_stubs_(isolate->GetJSEntryStubs()),
      embedded_code_range_(GetEmbeddedCodeRange(isolate)),
      required_code_ranges_capacity_(v8::Isolate::kMinCodePagesBufferSize) {}

V8Unwinder::~V8Unwinder() = default;

void V8Unwinder::InitializeModules() {
  // This function must be called only once.
  DCHECK(modules_.empty());

  // Add a module for the embedded code range.
  std::vector<std::unique_ptr<const base::ModuleCache::Module>> new_module;
  new_module.push_back(
      std::make_unique<V8Module>(embedded_code_range_, V8Module::kEmbedded));
  modules_.insert(new_module.front().get());
  module_cache()->UpdateNonNativeModules({}, std::move(new_module));
}

std::unique_ptr<base::UnwinderStateCapture>
V8Unwinder::CreateUnwinderStateCapture() {
  return std::make_unique<MemoryRanges>(required_code_ranges_capacity_);
}

// IMPORTANT NOTE: to avoid deadlock this function must not invoke any
// non-reentrant code that is also invoked by the target thread. In particular,
// no heap allocation or deallocation is permitted, including indirectly via use
// of DCHECK/CHECK or other logging statements.
void V8Unwinder::OnStackCapture(base::UnwinderStateCapture* capture_state) {
  MemoryRanges* code_ranges = static_cast<MemoryRanges*>(capture_state);
  required_code_ranges_capacity_ =
      CopyCodePages(code_ranges->size(), code_ranges->buffer());
  code_ranges->ShrinkSize(required_code_ranges_capacity_);
}

// Update the modules based on what was recorded in |code_ranges_|. The singular
// embedded code range was already added in in InitializeModules(). It is
// preserved by the algorithm below, which is why kNonEmbedded is
// unconditionally passed when creating new modules.
void V8Unwinder::UpdateModules(base::UnwinderStateCapture* capture_state) {
  MemoryRanges* code_ranges = static_cast<MemoryRanges*>(capture_state);
  MemoryRangeModuleCompare less_than;

  const auto is_embedded_code_range_module =
      [this](const base::ModuleCache::Module* module) {
        return module->GetBaseAddress() ==
                   reinterpret_cast<uintptr_t>(embedded_code_range_.start) &&
               module->GetSize() == embedded_code_range_.length_in_bytes;
      };

  std::vector<std::unique_ptr<const base::ModuleCache::Module>> new_modules;
  std::vector<const base::ModuleCache::Module*> defunct_modules;

  // Identify defunct modules and create new modules seen since the last
  // sample. Code ranges provided by V8 are in sorted order.
  v8::MemoryRange* const code_ranges_start = code_ranges->buffer();
  v8::MemoryRange* const code_ranges_end =
      code_ranges_start + code_ranges->size();
  CHECK(std::is_sorted(code_ranges_start, code_ranges_end, less_than));
  v8::MemoryRange* range_it = code_ranges_start;
  auto modules_it = modules_.begin();

  while (range_it != code_ranges_end && modules_it != modules_.end()) {
    if (less_than(*range_it, *modules_it)) {
      new_modules.push_back(
          std::make_unique<V8Module>(*range_it, V8Module::kNonEmbedded));
      modules_.insert(modules_it, new_modules.back().get());
      ++range_it;
    } else if (less_than(*modules_it, *range_it)) {
      // Avoid deleting the embedded code range module if it wasn't provided in
      // |code_ranges|. This could happen if |code_ranges| had insufficient
      // capacity when the code pages were copied.
      if (!is_embedded_code_range_module(*modules_it)) {
        defunct_modules.push_back(*modules_it);
        modules_it = modules_.erase(modules_it);
      } else {
        ++modules_it;
      }
    } else {
      // The range already has a module, so there's nothing to do.
      ++range_it;
      ++modules_it;
    }
  }

  while (range_it != code_ranges_end) {
    new_modules.push_back(
        std::make_unique<V8Module>(*range_it, V8Module::kNonEmbedded));
    modules_.insert(modules_it, new_modules.back().get());
    ++range_it;
  }

  while (modules_it != modules_.end()) {
    if (!is_embedded_code_range_module(*modules_it)) {
      defunct_modules.push_back(*modules_it);
      modules_it = modules_.erase(modules_it);
    } else {
      ++modules_it;
    }
  }

  module_cache()->UpdateNonNativeModules(defunct_modules,
                                         std::move(new_modules));
}

bool V8Unwinder::CanUnwindFrom(const base::Frame& current_frame) const {
  const base::ModuleCache::Module* module = current_frame.module;
  if (!module)
    return false;
  const auto loc = modules_.find(module);
  DCHECK(loc == modules_.end() || *loc == module);
  return loc != modules_.end();
}

base::UnwindResult V8Unwinder::TryUnwind(
    base::UnwinderStateCapture* capture_state,
    base::RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<base::Frame>* stack) {
  MemoryRanges* code_ranges = static_cast<MemoryRanges*>(capture_state);
  v8::RegisterState register_state;
  register_state.pc = reinterpret_cast<void*>(
      base::RegisterContextInstructionPointer(thread_context));
  register_state.sp = reinterpret_cast<void*>(
      base::RegisterContextStackPointer(thread_context));
  register_state.fp = reinterpret_cast<void*>(
      base::RegisterContextFramePointer(thread_context));

#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  if (!register_state.callee_saved)
    register_state.callee_saved = std::make_unique<v8::CalleeSavedRegisters>();
#endif
  CopyCalleeSavedRegisterFromRegisterContext(*thread_context,
                                             register_state.callee_saved.get());

  if (!v8::Unwinder::TryUnwindV8Frames(
          js_entry_stubs_, code_ranges->size(), code_ranges->buffer(),
          &register_state, reinterpret_cast<const void*>(stack_top))) {
    return base::UnwindResult::kAborted;
  }

  const uintptr_t prev_stack_pointer =
      base::RegisterContextStackPointer(thread_context);
  DCHECK_GT(reinterpret_cast<uintptr_t>(register_state.sp), prev_stack_pointer);
  DCHECK_LT(reinterpret_cast<uintptr_t>(register_state.sp), stack_top);

  base::RegisterContextInstructionPointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.pc);
  base::RegisterContextStackPointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.sp);
  base::RegisterContextFramePointer(thread_context) =
      reinterpret_cast<uintptr_t>(register_state.fp);

  CopyCalleeSavedRegisterToRegisterContext(register_state.callee_saved.get(),
                                           *thread_context);

  stack->emplace_back(
      base::RegisterContextInstructionPointer(thread_context),
      module_cache()->GetModuleForAddress(
          base::RegisterContextInstructionPointer(thread_context)));

  return base::UnwindResult::kUnrecognizedFrame;
}

size_t V8Unwinder::CopyCodePages(size_t capacity, v8::MemoryRange* code_pages) {
  return isolate_->CopyCodePages(capacity, code_pages);
}

// Synthetic build ids to use for V8 modules. The difference is in the digit
// after the leading 5's.
const char V8Unwinder::kV8EmbeddedCodeRangeBuildId[] =
    "5555555507284E1E874EFA4EB754964B999";
const char V8Unwinder::kV8CodeRangeBuildId[] =
    "5555555517284E1E874EFA4EB754964B999";

V8Unwinder::MemoryRanges::MemoryRanges(size_t size)
    : size_(size), ranges_(std::make_unique<v8::MemoryRange[]>(size)) {}

V8Unwinder::MemoryRanges::MemoryRanges::~MemoryRanges() = default;

void V8Unwinder::MemoryRanges::ShrinkSize(size_t size) {
  if (size < size_) {
    size_ = size;
  }
}

bool V8Unwinder::ModuleCompare::operator()(
    const base::ModuleCache::Module* a,
    const base::ModuleCache::Module* b) const {
  return std::make_pair(a->GetBaseAddress(), a->GetSize()) <
         std::make_pair(b->GetBaseAddress(), b->GetSize());
}
