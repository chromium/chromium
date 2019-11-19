// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
#define CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

class FakePasswordGenerationDriver
    : public autofill::mojom::PasswordGenerationDriver {
 public:
  FakePasswordGenerationDriver();

  ~FakePasswordGenerationDriver() override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<autofill::mojom::PasswordGenerationDriver>
          receiver);

  void Flush();

  // autofill::mojom::PasswordGenerationDriver:
  MOCK_METHOD1(GenerationAvailableForForm,
               void(const autofill::PasswordForm& password_form));
  MOCK_METHOD1(
      AutomaticGenerationAvailable,
      void(const autofill::password_generation::PasswordGenerationUIData&));
  MOCK_METHOD3(ShowPasswordEditingPopup,
               void(const gfx::RectF&,
                    const autofill::PasswordForm&,
                    uint32_t));
  MOCK_METHOD0(PasswordGenerationRejectedByTyping, void());
  MOCK_METHOD1(PresaveGeneratedPassword,
               void(const autofill::PasswordForm& password_form));
  MOCK_METHOD1(PasswordNoLongerGenerated,
               void(const autofill::PasswordForm& password_form));
  MOCK_METHOD0(FrameWasScrolled, void());
  MOCK_METHOD0(GenerationElementLostFocus, void());

 private:
  mojo::AssociatedReceiver<autofill::mojom::PasswordGenerationDriver> receiver_{
      this};

  DISALLOW_COPY_AND_ASSIGN(FakePasswordGenerationDriver);
};

#endif  // CHROME_RENDERER_AUTOFILL_FAKE_PASSWORD_GENERATION_DRIVER_H_
