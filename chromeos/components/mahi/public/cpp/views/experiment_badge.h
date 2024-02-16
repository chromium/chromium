// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_VIEWS_EXPERIMENT_BADGE_H_
#define CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_VIEWS_EXPERIMENT_BADGE_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace chromeos::mahi {

// The view for the experiment badge that will be used in `MahiMenuView` and
// `MahiPanelView`.
class COMPONENT_EXPORT(MAHI_PUBLIC_CPP) ExperimentBadge : public views::View {
  METADATA_HEADER(ExperimentBadge, views::View)

 public:
  ExperimentBadge();
  ExperimentBadge(const ExperimentBadge&) = delete;
  ExperimentBadge& operator=(const ExperimentBadge&) = delete;
  ~ExperimentBadge() override;

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

}  // namespace chromeos::mahi

#endif  // CHROMEOS_COMPONENTS_MAHI_PUBLIC_CPP_VIEWS_EXPERIMENT_BADGE_H_
