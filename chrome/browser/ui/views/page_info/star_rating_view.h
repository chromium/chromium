// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_STAR_RATING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_STAR_RATING_VIEW_H_

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
}  // namespace views

class NonAccessibleImageView;

class StarRatingView : public views::BoxLayoutView {
  METADATA_HEADER(StarRatingView, views::BoxLayoutView)

 public:
  static constexpr int kStarCount = 5;

  StarRatingView();
  StarRatingView(const StarRatingView&) = delete;
  StarRatingView& operator=(const StarRatingView&) = delete;
  ~StarRatingView() override;

  void SetRating(double rating);

  ui::VectorIconModel GetVectorIconModelForIndexForTesting(int index) const;
  std::u16string_view GetTextForTesting() const;

 private:
  // Returns the image model for the icon at |index| for |rating|.
  ui::ImageModel GetImageModel(double rating, int index);

  raw_ptr<views::Label> rating_label_ = nullptr;
  std::vector<raw_ptr<NonAccessibleImageView, VectorExperimental>> star_views_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_STAR_RATING_VIEW_H_
