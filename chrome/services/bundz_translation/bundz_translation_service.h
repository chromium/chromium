// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_BUNDZ_TRANSLATION_BUNDZ_TRANSLATION_SERVICE_H_
#define CHROME_SERVICES_BUNDZ_TRANSLATION_BUNDZ_TRANSLATION_SERVICE_H_

#include "chrome/services/bundz_translation/public/mojom/bundz_translation_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace bundz_translation {

class BundzTranslationService : public mojom::BundzTranslationService {
 public:
  explicit BundzTranslationService(
      mojo::PendingReceiver<mojom::BundzTranslationService> receiver);

  ~BundzTranslationService() override;

  BundzTranslationService(const BundzTranslationService&) = delete;
  BundzTranslationService& operator=(const BundzTranslationService&) = delete;

  // mojom::BundzTranslationService overrides.
  void CreateTranslator(
      const std::string& source_lang,
      const std::string& target_lang,
      mojo::PendingReceiver<bundz_translation::mojom::Translator> receiver,
      CreateTranslatorCallback callback) override;

  void CanTranslate(const std::string& source_lang,
                    const std::string& target_lang,
                    CanTranslateCallback callback) override;

 private:
  mojo::Receiver<mojom::BundzTranslationService> receiver_;
};

}  // namespace bundz_translation

#endif  // CHROME_SERVICES_BUNDZ_TRANSLATION_BUNDZ_TRANSLATION_SERVICE_H_
