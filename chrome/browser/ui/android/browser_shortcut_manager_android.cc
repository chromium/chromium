// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/browser_shortcut_manager_android.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/android/browser_command_delegate_android.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/android/accelerator_manager_android.h"
#include "ui/android/window_android.h"
#include "ui/base/accelerators/accelerator_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/BrowserShortcutManager_jni.h"

BrowserShortcutManagerAndroid::BrowserShortcutManagerAndroid(
    ui::AcceleratorManagerAndroid* accelerator_manager,
    std::unique_ptr<BrowserCommandDelegateAndroid> command_delegate)
    : accelerator_manager_(accelerator_manager),
      command_delegate_(std::move(command_delegate)) {
  CHECK(accelerator_manager_);
  CHECK(command_delegate_);

  for (const auto& mapping : GetAcceleratorList()) {
    ui::Accelerator accelerator(mapping.keycode, mapping.modifiers);
    CHECK(accelerator_table_.emplace(accelerator, mapping.command_id).second);
    accelerator_manager_->RegisterAccelerator(
        accelerator, ui::AcceleratorManager::kNormalPriority, this);
  }
}

BrowserShortcutManagerAndroid::~BrowserShortcutManagerAndroid() {
  for (const auto& entry : accelerator_table_) {
    accelerator_manager_->UnregisterAccelerator(entry.first, this);
  }
}

bool BrowserShortcutManagerAndroid::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  auto it = accelerator_table_.find(accelerator);
  if (it == accelerator_table_.end()) {
    return false;
  }

  int command_id = it->second;
  base::UmaHistogramSparse("Browser.Shortcuts.TriggeredCommandId", command_id);

  // ExecuteCommand() returns false when the command is disabled or unhandled
  // so the event keeps propagating (required for Escape).
  return command_delegate_->ExecuteCommand(command_id);
}

bool BrowserShortcutManagerAndroid::CanHandleAccelerators() const {
  return true;
}

int32_t BrowserShortcutManagerAndroid::PreHandleKeyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_key_event) {
  std::optional<ui::Accelerator> accelerator =
      ui::AcceleratorManagerAndroid::AcceleratorFromKeyEvent(j_key_event);
  if (!accelerator.has_value()) {
    return static_cast<int32_t>(ui::KeyEventPreHandleResult::kNotHandled);
  }

  if (accelerator_manager_->HasPriorityHandler(*accelerator)) {
    return static_cast<int32_t>(
        accelerator_manager_->ProcessAccelerator(*accelerator)
            ? ui::KeyEventPreHandleResult::kHandled
            : ui::KeyEventPreHandleResult::kNotHandled);
  }

  // In non-tabbed windows (Custom Tabs and Webapps), do not intercept shortcuts
  // pre-renderer so web content gets first priority (matching Desktop
  // BrowserView::PreHandleKeyboardEvent). If the page does not consume the
  // event, supported single-tab shortcuts still execute post-renderer via
  // AcceleratorPressed.
  if (!command_delegate_->IsTabbedBrowserCommandSupported()) {
    return static_cast<int32_t>(ui::KeyEventPreHandleResult::kNotHandled);
  }

  auto it = accelerator_table_.find(*accelerator);
  if (it == accelerator_table_.end() ||
      command_delegate_->IsReservedCommand(it->second)) {
    return static_cast<int32_t>(
        accelerator_manager_->ProcessAccelerator(*accelerator)
            ? ui::KeyEventPreHandleResult::kHandled
            : ui::KeyEventPreHandleResult::kNotHandled);
  }

  CHECK_EQ(accelerator->key_state(), ui::Accelerator::KeyState::PRESSED);
  return static_cast<int32_t>(
      ui::KeyEventPreHandleResult::kNotHandledIsShortcut);
}

void BrowserShortcutManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

static int64_t JNI_BrowserShortcutManager_Init(
    JNIEnv* env,
    ui::WindowAndroid* window_android,
    const jni_zero::JavaRef<jobject>& jcommand_delegate) {
  CHECK(window_android);

  ui::AcceleratorManagerAndroid* accelerator_manager =
      ui::AcceleratorManagerAndroid::FromWindow(window_android);

  auto command_delegate =
      std::make_unique<BrowserCommandDelegateAndroid>(jcommand_delegate);

  return reinterpret_cast<int64_t>(new BrowserShortcutManagerAndroid(
      accelerator_manager, std::move(command_delegate)));
}

DEFINE_JNI(BrowserShortcutManager)
