// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEXT_ENCODER_TEXT_ENCODER_H_
#define COMPONENTS_TEXT_ENCODER_TEXT_ENCODER_H_

#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"

namespace gin {
class Arguments;
}

namespace text_encoder {

// Provides TextEncoder object to the V8 Javascript.
// This class is a parallel to blink::TextEncoder, which does the same for
// any blink renderer.
// Note that this only supports UTF-8 encoding.
class TextEncoder : public gin::Wrappable<TextEncoder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<TextEncoder> Create(
      v8::Local<v8::Context> context,
      std::unique_ptr<TextEncoder>* text_encoder);

  ~TextEncoder() override = default;
  TextEncoder(const TextEncoder&) = delete;
  TextEncoder& operator=(const TextEncoder&) = delete;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  //
  // Methods exposed to Javascript.
  // Note: gin::Wrappable's bound methods need to be public.
  //

  // Encodes a Javascript string into a v8::Uint8Array.
  // See third_party/blink/renderer/modules/encoding/text_encoder.idl.
  // Behavior is the same as the Blink version:
  //  1. only the first argument is encoded.
  //  2. extra arguments are ignored.
  //  3. returns empty array if no arguments passed.
  //  4. if the first argument isn't a string, it's stringified.
  void Encode(gin::Arguments* arguments);

  //
  // End of methods exposed to Javascript.
  //

 private:
  TextEncoder() = default;
};

}  // namespace text_encoder

#endif  // COMPONENTS_TEXT_ENCODER_TEXT_ENCODER_H_
