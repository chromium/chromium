// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_EXPIRED_CONTAINER_WARNING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_EXPIRED_CONTAINER_WARNING_VIEW_H_

#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

class Profile;

class CrostiniExpiredContainerWarningView
    : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(CrostiniExpiredContainerWarningView);

  static void Show(Profile* profile, std::vector<base::OnceClosure> callbacks);

 private:
  CrostiniExpiredContainerWarningView(Profile* profile,
                                      std::vector<base::OnceClosure> callbacks);
  ~CrostiniExpiredContainerWarningView() override;

  Profile* const profile_;  // Not owned.
  std::vector<base::OnceClosure> callbacks_;

  base::WeakPtrFactory<CrostiniExpiredContainerWarningView> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_EXPIRED_CONTAINER_WARNING_VIEW_H_
