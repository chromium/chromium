// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_FAKE_OWNER_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_FAKE_OWNER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

class FakeOwner : public ContentSettingBubbleModel::Owner {
 public:
  explicit FakeOwner(ContentSettingBubbleModel& model, int option)
      : model_(model), selected_radio_option_(option) {}
  ~FakeOwner() override = default;

  int GetSelectedRadioOption() override;

  static std::unique_ptr<FakeOwner> Create(ContentSettingBubbleModel& model,
                                           int initial_value) {
    std::unique_ptr<FakeOwner> owner =
        std::make_unique<FakeOwner>(model, initial_value);
    model.set_owner(owner.get());
    return owner;
  }

  void SetSelectedRadioOptionAndCommit(int option) {
    selected_radio_option_ = option;
    model_->CommitChanges();
  }

 private:
  const raw_ref<ContentSettingBubbleModel> model_;
  int selected_radio_option_;
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_FAKE_OWNER_H_
