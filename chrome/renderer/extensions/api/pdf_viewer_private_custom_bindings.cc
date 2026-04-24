// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/pdf_viewer_private_custom_bindings.h"

#include <optional>
#include <set>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "extensions/renderer/script_context.h"
#include "gin/array_buffer.h"
#include "gin/converter.h"
#include "pdf/mojom/pdf.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "ui/gfx/skia_span_util.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2),
              "This file should not be compiled if enable_pdf_ink2 is off");

namespace extensions {

namespace {

void ThrowException(v8::Isolate* isolate, const char* exception) {
  isolate->ThrowException(
      v8::Exception::Error(gin::StringToSymbol(isolate, exception)));
}

void ThrowTypeException(v8::Isolate* isolate, const char* exception) {
  isolate->ThrowException(
      v8::Exception::TypeError(gin::StringToSymbol(isolate, exception)));
}

v8::Local<v8::ArrayBuffer> GetSerializedFont(v8::Isolate* isolate,
                                             sk_sp<SkTypeface> font) {
  CHECK(font);
  sk_sp<SkData> serialized_font =
      font->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  if (!serialized_font) {
    return {};
  }

  base::span<const uint8_t> serialized_font_data =
      gfx::SkDataToSpan(serialized_font);
  v8::Local<v8::ArrayBuffer> buffer =
      v8::ArrayBuffer::New(isolate, serialized_font_data.size());
  gin::ArrayBuffer gin_buffer(buffer);
  gin_buffer.span().copy_from_nonoverlapping(serialized_font_data);
  return buffer;
}

bool SetTypefaceInArray(v8::Isolate* isolate,
                        v8::Local<v8::Array>& typefaces_result,
                        uint32_t index,
                        sk_sp<SkTypeface> font) {
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::ArrayBuffer> serialized_font = GetSerializedFont(isolate, font);
  if (serialized_font.IsEmpty()) {
    return false;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> typeface = v8::Object::New(isolate);
  typeface
      ->Set(context, gin::StringToSymbol(isolate, "uniqueId"),
            v8::Integer::NewFromUnsigned(isolate, font->uniqueID()))
      .Check();
  typeface
      ->Set(context, gin::StringToSymbol(isolate, "serializedTypeface"),
            serialized_font)
      .Check();
  typefaces_result->Set(context, index, typeface).Check();
  return true;
}

v8::Local<v8::Object> CreateResult(v8::Isolate* isolate,
                                   v8::Local<v8::Array> typefaces,
                                   v8::Local<v8::Object> text_info) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result->Set(context, gin::StringToSymbol(isolate, "typefaces"), typefaces)
      .Check();
  result->Set(context, gin::StringToSymbol(isolate, "mojoTextInfo"), text_info)
      .Check();
  return result;
}

}  // namespace

PdfViewerPrivateCustomBindings::PdfViewerPrivateCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

PdfViewerPrivateCustomBindings::~PdfViewerPrivateCustomBindings() = default;

void PdfViewerPrivateCustomBindings::AddRoutes() {
  RouteHandlerFunction(
      "GetTextInfo", "pdfViewerPrivate",
      base::BindRepeating(&PdfViewerPrivateCustomBindings::GetTextInfo,
                          base::Unretained(this)));
}

void PdfViewerPrivateCustomBindings::GetTextInfo(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 2);
  CHECK(args[1]->IsArray());

  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  blink::WebElement element = blink::WebElement::FromV8Value(isolate, args[0]);
  v8::Local<v8::Array> known_font_ids = args[1].As<v8::Array>();

  // Read the <textarea> arg and call GetTextInfo() on it.
  auto form_control = element.DynamicTo<blink::WebFormControlElement>();
  if (form_control.IsNull()) {
    ThrowTypeException(isolate, "Missing <textarea> element");
    return;
  }
  std::optional<blink::WebFormControlElement::TextInfo> maybe_text_info =
      form_control.GetTextInfo();
  if (!maybe_text_info.has_value()) {
    ThrowException(isolate, "Failed to get text info");
    return;
  }
  const std::vector<blink::WebFormControlElement::TextRunInfo>& text_runs =
      maybe_text_info->text_runs;

  // Read the knownFontIds arg into seen_fonts.
  std::set<uint32_t> seen_fonts;
  v8::Maybe<void> iterate_result = known_font_ids->Iterate(
      context,
      [](uint32_t i, v8::Local<v8::Value> element,
         void* data) -> v8::Array::CallbackResult {
        if (!element->IsUint32()) {
          return v8::Array::CallbackResult::kException;
        }
        reinterpret_cast<std::set<uint32_t>*>(data)->insert(
            element.As<v8::Uint32>()->Value());
        return v8::Array::CallbackResult::kContinue;
      },
      &seen_fonts);
  if (iterate_result.IsEmpty()) {
    ThrowException(isolate,
                   "Iterating knownFontIds failed (elements must be uint32)");
    return;
  }

  // Serialize the fonts that weren't in knownFontIds.
  v8::Local<v8::Array> typefaces_result = v8::Array::New(isolate);
  uint32_t typeface_index = 0;
  for (const blink::WebFormControlElement::TextRunInfo& text_run : text_runs) {
    for (const blink::WebFormControlElement::TypefaceRunInfo& info :
         text_run.typeface_runs) {
      const bool inserted = seen_fonts.insert(info.typeface->uniqueID()).second;
      if (!inserted) {
        continue;
      }
      if (!SetTypefaceInArray(isolate, typefaces_result, typeface_index++,
                              info.typeface)) {
        ThrowException(isolate, "Cannot serialize font");
        return;
      }
    }
  }

  // Fill the mojo struct with the GetTextInfo() results.
  auto text_info_mojo = pdf::mojom::InkTextInfo::New();
  text_info_mojo->effective_zoom = maybe_text_info->effective_zoom;
  for (const blink::WebFormControlElement::TextRunInfo& text_run : text_runs) {
    auto text_run_mojo = pdf::mojom::InkTextRun::New();
    text_run_mojo->location = text_run.location;
    for (const blink::WebFormControlElement::TypefaceRunInfo& info :
         text_run.typeface_runs) {
      auto typeface_run_mojo = pdf::mojom::InkTypefaceRun::New();
      typeface_run_mojo->typeface_id = info.typeface->uniqueID();
      typeface_run_mojo->is_horizontal = info.is_horizontal;
      for (const blink::WebFormControlElement::GlyphInfo& glyph : info.glyphs) {
        auto glyph_mojo = pdf::mojom::InkGlyphInfo::New();
        glyph_mojo->glyph = glyph.glyph;
        glyph_mojo->offset = glyph.offset;
        glyph_mojo->total_advance = glyph.total_advance;
        typeface_run_mojo->glyphs.push_back(std::move(glyph_mojo));
      }
      text_run_mojo->typeface_runs.push_back(std::move(typeface_run_mojo));
    }
    text_info_mojo->text_runs.push_back(std::move(text_run_mojo));
  }
  // Serialize the mojo struct into a v8::ArrayBuffer.
  std::vector<uint8_t> serialized_text_info =
      pdf::mojom::InkTextInfo::Serialize(&text_info_mojo);
  v8::Local<v8::ArrayBuffer> text_info_result =
      v8::ArrayBuffer::New(isolate, serialized_text_info.size());
  gin::ArrayBuffer gin_buffer(text_info_result);
  gin_buffer.span().copy_from_nonoverlapping(serialized_text_info);

  args.GetReturnValue().Set(
      CreateResult(isolate, typefaces_result, text_info_result));
}

}  // namespace extensions
