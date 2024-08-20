// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TEST_TEST_CONFIRM_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TEST_TEST_CONFIRM_BUBBLE_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"

// A test version of the model for confirmation bubbles.
class TestConfirmBubbleModel : public ConfirmBubbleModel {
 public:
  // Parameters may be NULL depending on the needs of the test.
  TestConfirmBubbleModel(bool* model_deleted,
                         bool* accept_clicked,
                         bool* cancel_clicked,
                         bool* link_clicked);

  TestConfirmBubbleModel(const TestConfirmBubbleModel&) = delete;
  TestConfirmBubbleModel& operator=(const TestConfirmBubbleModel&) = delete;

  ~TestConfirmBubbleModel() override;

  // ConfirmBubbleModel overrides:
  std::u16string GetTitle() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(ui::mojom::DialogButton button) const override;
  void Accept() override;
  void Cancel() override;
  std::u16string GetLinkText() const override;
  void OpenHelpPage() override;

 private:
  raw_ptr<bool> model_deleted_;
  raw_ptr<bool> accept_clicked_;
  raw_ptr<bool> cancel_clicked_;
  raw_ptr<bool> link_clicked_;
};

#endif  // CHROME_BROWSER_UI_TEST_TEST_CONFIRM_BUBBLE_MODEL_H_
