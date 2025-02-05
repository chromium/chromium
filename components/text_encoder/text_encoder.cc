// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/text_encoder/text_encoder.h"

#include <cstring>
#include <memory>

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/public/wrapper_info.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-typed-array.h"

namespace text_encoder {

// static
gin::WrapperInfo TextEncoder::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
gin::Handle<TextEncoder> TextEncoder::Create(
    v8::Local<v8::Context> context,
    std::unique_ptr<TextEncoder>* text_encoder) {
  text_encoder->reset(new TextEncoder());
  return gin::CreateHandle(context->GetIsolate(), text_encoder->get());
}

gin::ObjectTemplateBuilder TextEncoder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  // Note: We do not support TextEncoder::encodeInto.
  return gin::Wrappable<TextEncoder>::GetObjectTemplateBuilder(isolate)
      .SetMethod("encode", &TextEncoder::Encode);
}

void TextEncoder::Encode(gin::Arguments* arguments) {
  v8::Isolate* isolate = arguments->isolate();
  CHECK(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::LocalVector<v8::Value> args = arguments->GetAll();
  std::string input;
  if (args.size() > 0u) {
    bool valid_string = false;
    v8::Local<v8::String> v8_input;
    if (args[0]->IsString()) {
      v8_input = args[0].As<v8::String>();
      valid_string = true;
    } else if (args[0]
                   ->ToString(isolate->GetCurrentContext())
                   .ToLocal(&v8_input)) {
      valid_string = true;
    }
    if (valid_string) {
      gin::ConvertFromV8(isolate, v8_input, &input);
    }
  }

  int num_bytes = input.size();
  void* buffer =
      gin::ArrayBufferAllocator::SharedInstance()->Allocate(num_bytes);
  auto deleter = [](void* buffer, size_t length, void* data) {
    gin::ArrayBufferAllocator::SharedInstance()->Free(buffer, length);
  };
  std::unique_ptr<v8::BackingStore> backing_store =
      v8::ArrayBuffer::NewBackingStore(buffer, num_bytes, deleter, nullptr);

  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, std::move(backing_store));
  if (num_bytes) {
    CHECK(array_buffer->Data());
    memcpy(array_buffer->Data(), input.c_str(), num_bytes);
  }
  v8::Local<v8::Uint8Array> result =
      v8::Uint8Array::New(array_buffer, 0, num_bytes);
  arguments->GetFunctionCallbackInfo()->GetReturnValue().Set(result);
}

}  // namespace text_encoder
