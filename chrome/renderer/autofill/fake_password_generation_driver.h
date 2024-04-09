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
  MOCK_METHOD(void,
              AutomaticGenerationAvailable,
              (const autofill::password_generation::PasswordGenerationUIData&),
              (override));
  MOCK_METHOD(void,
              ShowPasswordEditingPopup,
              (const gfx::RectF&,
               const autofill::FormData&,
               autofill::FieldRendererId,
               const std::u16string&),
              (override));
  MOCK_METHOD(void, PasswordGenerationRejectedByTyping, (), (override));
  MOCK_METHOD(void,
              PresaveGeneratedPassword,
              (const autofill::FormData& form_data,
               const std::u16string& generated_password),
              (override));
  MOCK_METHOD(void,
              PasswordNoLongerGenerated,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(void, FrameWasScrolled, (), (override));
  MOCK_METHOD(void, GenerationElementLostFocus, (), (override));

 private:
  mojo::AssociatedReceiver<autofill::mojom::PasswordGenerationDriver> receiver_{
      this};
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
