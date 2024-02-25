// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/multi_image_container.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

TEST(MultiImageContainerTest, SetImages) {
  std::unique_ptr<views::View> images;
  MultiImageContainer container;
  images = container.CreateView();
  EXPECT_EQ(images->children().size(), 1u);

  size_t number_of_images = 5;  // some arbitrary number.
  std::vector<ui::ImageModel> models(number_of_images);
  container.SetImages(models);
  EXPECT_EQ(images->children().size(), number_of_images);

  number_of_images = 3;  // some arbitrary number.
  models.resize(number_of_images);
  container.SetImages(models);
  EXPECT_EQ(images->children().size(), number_of_images);
}
