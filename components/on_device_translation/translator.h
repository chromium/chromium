// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_TRANSLATOR_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_TRANSLATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/on_device_translation/input_budget.h"
#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"
#include "third_party/blink/public/mojom/on_device_translation/translator.mojom.h"

namespace on_device_translation {

// The browser-side implementation of `blink::mojom::Translator`, which
// exposes the `Translate()` method to do translation.
class Translator : public blink::mojom::Translator {
 public:
  // `input_budget` is shared by the translators of an origin, and limits the
  // input that waits for the translation service. Null means no limit.
  Translator(
      const base::RepeatingCallback<bool()>& can_translate_callback,
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingRemote<on_device_translation::mojom::OnDeviceTranslator>
          remote,
      scoped_refptr<InputBudget> input_budget);

  Translator(const Translator&) = delete;
  Translator& operator=(const Translator&) = delete;

  ~Translator() override;

  // `blink::mojom::Translator` implementation.
  void Translate(const std::string& input,
                 mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                     pending_responder) override;
  void TranslateStreaming(
      const std::string& input,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder) override;

 private:
  using ServiceRemote =
      base::RefCountedData<mojo::Remote<mojom::OnDeviceTranslator>>;

  // Holds the service pipe and input budget reservation while a translation
  // call waits for the service to reply.
  struct InFlightCall;

  // Callback for on_device_translation chunks requested by TranslateStreaming.
  void TranslateStreamingCallback(scoped_refptr<InFlightCall> call,
                                  mojo::RemoteSetElementId responder_id,
                                  int total_translations,
                                  const std::optional<std::string>& output);
  // Callback for `SplitSentences` that invokes `Translate` on each sentence.
  void SplitSentencesCallback(
      mojo::Remote<blink::mojom::ModelStreamingResponder> responder,
      scoped_refptr<InFlightCall> call,
      const std::vector<std::string>& sentences);
  // Reserves the size of `input` in `input_budget_`, then sends it to the
  // service. The call can wait in the browser for the budget.
  void ReserveInputAndSend(
      const std::string& input,
      mojo::Remote<blink::mojom::ModelStreamingResponder> responder,
      bool streaming);
  void OnInputReserved(
      const std::string& input,
      mojo::Remote<blink::mojom::ModelStreamingResponder> responder,
      bool streaming,
      std::optional<InputBudget::Reservation> reservation);
  // Sends `input` to the service, to translate it, or with `streaming`, to
  // split it into sentences first.
  void SendToService(
      const std::string& input,
      mojo::Remote<blink::mojom::ModelStreamingResponder> responder,
      bool streaming,
      std::optional<InputBudget::Reservation> reservation);
  // Check shared prerequisites for Translate and TranslateStreaming;
  // return true if all were satisfied, false otherwise.
  bool VerifyPrerequisites(
      const std::string& input,
      mojo::Remote<blink::mojom::ModelStreamingResponder>& responder);

  base::RepeatingCallback<bool()> can_translate_callback_;
  const std::string source_lang_;
  const std::string target_lang_;
  scoped_refptr<ServiceRemote> service_;
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;
  // The number of remaining Translate callbacks expected for each responder.
  base::flat_map<mojo::RemoteSetElementId, int> pending_translations_;
  const scoped_refptr<InputBudget> input_budget_;
  base::WeakPtrFactory<Translator> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_TRANSLATOR_H_
