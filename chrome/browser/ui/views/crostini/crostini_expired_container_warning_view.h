// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_EXPIRED_CONTAINER_WARNING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_EXPIRED_CONTAINER_WARNING_VIEW_H_

#include "base/memory/raw_ptr.h"
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
  METADATA_HEADER(CrostiniExpiredContainerWarningView,
                  views::BubbleDialogDelegateView)

 public:
  static void Show(Profile* profile, base::OnceClosure callback);

 private:
  CrostiniExpiredContainerWarningView(Profile* profile,
                                      base::OnceClosure callback);
  ~CrostiniExpiredContainerWarningView() override;

  const raw_ptr<Profile> profile_;  // Not owned.
  std::vector<base::OnceClosure> callbacks_;

  base::WeakPtrFactory<CrostiniExpiredContainerWarningView> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CROSTINI_CROSTINI_EXPIRED_CONTAINER_WARNING_VIEW_H_
