// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/translator.h"

#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/metrics.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"
#include "url/origin.h"

namespace on_device_translation {

namespace {

// Returns whether `input` contains any character beyond Unicode whitespace and
// control characters, including non-ASCII spaces such as U+00A0 and U+3000.
bool ContainsTranslatableContent(std::string_view input) {
  for (size_t index = 0; index < input.size(); ++index) {
    base_icu::UChar32 code_point;
    if (!base::ReadUnicodeCharacter(input, &index, &code_point)) {
      // Leave invalid UTF-8 to the translation service.
      return true;
    }
    if (!base::IsUnicodeWhitespace(code_point) &&
        !base::IsUnicodeControl(code_point)) {
      return true;
    }
  }
  return false;
}

}  // namespace

Translator::Translator(
    const base::RepeatingCallback<bool()>& can_translate_callback,
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingRemote<on_device_translation::mojom::OnDeviceTranslator>
        remote)
    : can_translate_callback_(can_translate_callback),
      source_lang_(source_lang),
      target_lang_(target_lang),
      translator_remote_(std::move(remote)) {}

Translator::~Translator() = default;

bool Translator::VerifyPrerequisites(
    const std::string& input,
    mojo::Remote<blink::mojom::ModelStreamingResponder>& responder) {
  if (!can_translate_callback_.Run()) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return false;
  }

  RecordTranslatorApiCallForLanguagePair("Translate", source_lang_,
                                         target_lang_);
  RecordTranslatorApiTranslationLength(source_lang_, target_lang_,
                                       input.size());

  // The renderer is not trusted, so check the size here, before the input
  // reaches the translation service.
  const int max_input_size = kTranslationAPIMaxInputSizeBytes.Get();
  if (max_input_size > 0 &&
      input.size() > static_cast<size_t>(max_input_size)) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(
            base::saturated_cast<uint32_t>(input.size()),
            static_cast<uint32_t>(max_input_size)));
    return false;
  }

  // https://github.com/webmachinelearning/translation-api/pull/38: "If |input|
  // is the empty string, or otherwise consists of no translatable content
  // (e.g., only contains whitespace, or control characters), then the resulting
  // translation should be |input|. In such cases, |sourceLanguage| and
  // |targetLanguage| should be ignored."
  if (!ContainsTranslatableContent(input)) {
    responder->OnStreaming(input);
    responder->OnCompletion(/*context_info=*/nullptr);
    return false;
  }
  return true;
}

void Translator::Translate(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));

  if (!VerifyPrerequisites(input, responder)) {
    return;
  }

  if (translator_remote_.is_connected()) {
    translator_remote_->Translate(
        input, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                   base::BindOnce(
                       [](mojo::Remote<blink::mojom::ModelStreamingResponder>
                              responder,
                          const std::optional<std::string>& output) {
                         if (!output) {
                           responder->OnError(
                               blink::mojom::ModelStreamingResponseStatus::
                                   kErrorGenericFailure,
                               /*quota_error_info=*/nullptr);
                           return;
                         }
                         responder->OnStreaming(output.value());
                         responder->OnCompletion(/*context_info=*/nullptr);
                       },
                       std::move(responder)),
                   std::nullopt));
  } else {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
  }
}

void Translator::SplitSentencesCallback(
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder,
    const std::vector<std::string>& sentences) {
  if (sentences.empty()) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return;
  }
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(responder));

  pending_translations_[responder_id] = sentences.size();
  const int total_translations = sentences.size();
  for (const auto& sentence : sentences) {
    // Run the reply callback with nullopt if the service disconnects before
    // answering, so the stream rejects right away.
    translator_remote_->Translate(
        sentence, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                      base::BindOnce(&Translator::TranslateStreamingCallback,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     responder_id, total_translations),
                      std::nullopt));
  }
}
void Translator::TranslateStreamingCallback(
    mojo::RemoteSetElementId responder_id,
    int total_translations,
    const std::optional<std::string>& output) {
  auto it = pending_translations_.find(responder_id);

  blink::mojom::ModelStreamingResponder* responder_ptr =
      responder_set_.Get(responder_id);

  // This should only happen after the responder disconnected.
  if (it == pending_translations_.end()) {
    CHECK(!responder_ptr);
    return;
  }

  // This indicates that the responder disconnected.
  if (!responder_ptr) {
    pending_translations_.erase(it);
    return;
  }

  if (!output) {
    responder_ptr->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    responder_set_.Remove(responder_id);
    pending_translations_.erase(it);
    return;
  }

  // Each Translate() call strips leading and trailing whitespace.
  // If this is the first sentence we are streaming, do not prepend a space.
  // Otherwise, prepend a space to the chunk.
  const int pending_translations = it->second;
  if (pending_translations == total_translations) {
    responder_ptr->OnStreaming(output.value());
  } else {
    responder_ptr->OnStreaming(base::StrCat({" ", output.value()}));
  }

  if (--it->second == 0) {
    responder_ptr->OnCompletion(/*context_info=*/nullptr);
    responder_set_.Remove(responder_id);
    pending_translations_.erase(it);
  }
}

void Translator::TranslateStreaming(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
      std::move(pending_responder));

  if (!VerifyPrerequisites(input, responder)) {
    return;
  }

  if (!translator_remote_.is_connected()) {
    responder->OnError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure,
        /*quota_error_info=*/nullptr);
    return;
  }

  translator_remote_->SplitSentences(
      input,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&Translator::SplitSentencesCallback,
                         weak_ptr_factory_.GetWeakPtr(), std::move(responder)),
          std::vector<std::string>()));
}

}  // namespace on_device_translation
