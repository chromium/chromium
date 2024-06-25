// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_
#define CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"

// List model that controls which item is added to WebAuthN UI views.
// HoverListModel is observed by Observer which represents the actual
// UI view component.
class HoverListModel {
 public:
  HoverListModel() = default;
  HoverListModel(const HoverListModel&) = delete;
  HoverListModel& operator=(const HoverListModel&) = delete;
  virtual ~HoverListModel() = default;

  virtual std::vector<int> GetButtonTags() const = 0;
  virtual std::u16string GetItemText(int item_tag) const = 0;
  virtual std::u16string GetDescriptionText(int item_tag) const = 0;
  // GetItemIcon may return nullptr to indicate that no icon should be added.
  // This is distinct from using an empty icon as the latter will still take up
  // as much space as any other icon.
  virtual ui::ImageModel GetItemIcon(int item_tag) const = 0;
  virtual bool IsButtonEnabled(int item_tag) const = 0;
  virtual void OnListItemSelected(int item_tag) = 0;
  virtual size_t GetPreferredItemCount() const = 0;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_HOVER_LIST_MODEL_H_
