// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_TRANSLATOR_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_TRANSLATOR_H_

#include "components/on_device_translation/public/mojom/translator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace on_device_translation {

// A fake implementation of the on_device_translation::mojom::OnDeviceTranslator
// mojo interface for use in tests.
class FakeTranslator : public on_device_translation::mojom::OnDeviceTranslator {
 public:
  explicit FakeTranslator(
      mojo::PendingReceiver<on_device_translation::mojom::OnDeviceTranslator>
          receiver);
  ~FakeTranslator() override;

  // on_device_translation::mojom::OnDeviceTranslator:
  void Translate(const std::string& input, TranslateCallback callback) override;
  void SplitSentences(const std::string& input,
                      SplitSentencesCallback callback) override;

 private:
  mojo::Receiver<on_device_translation::mojom::OnDeviceTranslator> receiver_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_TEST_FAKE_TRANSLATOR_H_
