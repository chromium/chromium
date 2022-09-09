// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_

#include <string>
#include <vector>

#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/unique_ids.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

class FakePasswordGenerationDriver
    : public autofill::mojom::PasswordGenerationDriver {
 public:
  FakePasswordGenerationDriver();

  FakePasswordGenerationDriver(const FakePasswordGenerationDriver&) = delete;
  FakePasswordGenerationDriver& operator=(const FakePasswordGenerationDriver&) =
      delete;

  ~FakePasswordGenerationDriver() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordGenerationDriver>
          receiver);

  void Flush();

  // autofill::mojom::PasswordGenerationDriver:
  MOCK_METHOD1(
      AutomaticGenerationAvailable,
      void(const autofill::password_generation::PasswordGenerationUIData&));
  MOCK_METHOD4(ShowPasswordEditingPopup,
               void(const gfx::RectF&,
                    const autofill::FormData&,
                    autofill::FieldRendererId,
                    const std::u16string&));
  MOCK_METHOD0(PasswordGenerationRejectedByTyping, void());
  MOCK_METHOD2(PresaveGeneratedPassword,
               void(const autofill::FormData& form_data,
                    const std::u16string& generated_password));
  MOCK_METHOD1(PasswordNoLongerGenerated,
               void(const autofill::FormData& form_data));
  MOCK_METHOD0(FrameWasScrolled, void());
  MOCK_METHOD0(GenerationElementLostFocus, void());

 private:
  mojo::AssociatedReceiver<autofill::mojom::PasswordGenerationDriver> receiver_{
      this};
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
