// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_confirm_bubble_model.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "ui/base/mojom/dialog_button.mojom.h"

TestConfirmBubbleModel::TestConfirmBubbleModel(bool* model_deleted,
                                               bool* accept_clicked,
                                               bool* cancel_clicked,
                                               bool* link_clicked)
    : model_deleted_(model_deleted),
      accept_clicked_(accept_clicked),
      cancel_clicked_(cancel_clicked),
      link_clicked_(link_clicked) {
}

TestConfirmBubbleModel::~TestConfirmBubbleModel() {
  if (model_deleted_)
    *model_deleted_ = true;
}

std::u16string TestConfirmBubbleModel::GetTitle() const {
  return u"Test";
}

std::u16string TestConfirmBubbleModel::GetMessageText() const {
  return u"Test Message";
}

std::u16string TestConfirmBubbleModel::GetButtonLabel(
    ui::mojom::DialogButton button) const {
  return button == ui::mojom::DialogButton::kOk ? u"OK" : u"Cancel";
}

void TestConfirmBubbleModel::Accept() {
  if (accept_clicked_)
    *accept_clicked_ = true;
}

void TestConfirmBubbleModel::Cancel() {
  if (cancel_clicked_)
    *cancel_clicked_ = true;
}

std::u16string TestConfirmBubbleModel::GetLinkText() const {
  return u"Link";
}

void TestConfirmBubbleModel::OpenHelpPage() {
  if (link_clicked_)
    *link_clicked_ = true;
}
