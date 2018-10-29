// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_
#define COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/common/previews_state.h"

namespace previews {

// A representation of previews information related to a navigation.
// TODO(ryansturm): rename this to remove UserData.
class PreviewsUserData {
 public:
  PreviewsUserData(uint64_t page_id);
  ~PreviewsUserData();

  PreviewsUserData(const PreviewsUserData& previews_user_data);

  // A session unique ID related to this navigation.
  uint64_t page_id() const { return page_id_; }

  // Sets a data savings inflation percent value to use instead of the default
  // if there is a committed preview. Note that this is expected to be used for
  // specific preview types (such as NoScript) that don't have better data use
  // estimation information.
  void SetDataSavingsInflationPercent(int inflation_percent) {
    data_savings_inflation_percent_ = inflation_percent;
  }

  // Returns the data savings inflation percent to use for this navigation
  // instead of the default if it is not 0.
  int data_savings_inflation_percent() {
    return data_savings_inflation_percent_;
  }

  // Whether a lite page preview was prevented from being shown due to the
  // blacklist.
  bool black_listed_for_lite_page() const {
    return black_listed_for_lite_page_;
  }
  void set_black_listed_for_lite_page(bool black_listed_for_lite_page) {
    black_listed_for_lite_page_ = black_listed_for_lite_page;
  }

  // Sets that the page load received the Cache-Control:no-transform
  // directive. Expected to be set upon receiving a committed response.
  void SetCacheControlNoTransformDirective() {
    cache_control_no_transform_directive_ = true;
  }

  // Returns whether the Cache-Control:no-transform directive has been
  // detected for the request. Should not be called prior to receiving
  // a committed response.
  bool cache_control_no_transform_directive() {
    return cache_control_no_transform_directive_;
  }

  // Sets the committed previews type. Should only be called once.
  void SetCommittedPreviewsType(previews::PreviewsType previews_type);

  // The committed previews type, if any. Otherwise PreviewsType::NONE.
  previews::PreviewsType committed_previews_type() const {
    return committed_previews_type_;
  }

  // Whether there is a committed previews type.
  bool HasCommittedPreviewsType() const {
    return committed_previews_type_ != previews::PreviewsType::NONE;
  }

  // Whether an offline preview is being served.
  void set_offline_preview_used(bool offline_preview_used) {
    offline_preview_used_ = offline_preview_used;
  }
  bool offline_preview_used() { return offline_preview_used_; }

  // The PreviewsState that was allowed for the navigation.
  content::PreviewsState allowed_previews_state() {
    return allowed_previews_state_;
  }
  void set_allowed_previews_state(
      content::PreviewsState allowed_previews_state) {
    allowed_previews_state_ = allowed_previews_state;
  }

  // The PreviewsState that was committed for the navigation.
  content::PreviewsState committed_previews_state() {
    return committed_previews_state_;
  }
  void set_committed_previews_state(
      content::PreviewsState committed_previews_state) {
    committed_previews_state_ = committed_previews_state;
  }

 private:

  // A session unique ID related to this navigation.
  const uint64_t page_id_;
  // A previews data savings inflation percent for the navigation if not 0.
  int data_savings_inflation_percent_ = 0;
  // Whether the origin provided a no-transform directive.
  bool cache_control_no_transform_directive_ = false;

  // Whether an offline preview is being served.
  bool offline_preview_used_ = false;

  // Whether a lite page preview was prevented from being shown due to the
  // blacklist.
  bool black_listed_for_lite_page_ = false;

  // The committed previews type, if any.
  previews::PreviewsType committed_previews_type_ = PreviewsType::NONE;

  // The PreviewsState that was allowed for the navigation.
  content::PreviewsState allowed_previews_state_ =
      content::PREVIEWS_UNSPECIFIED;

  // The PreviewsState that was committed for the navigation.
  content::PreviewsState committed_previews_state_ = content::PREVIEWS_OFF;

  DISALLOW_ASSIGN(PreviewsUserData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_PREVIEWS_USER_DATA_H_
