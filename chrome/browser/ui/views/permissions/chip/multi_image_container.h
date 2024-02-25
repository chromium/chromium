// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_MULTI_IMAGE_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_MULTI_IMAGE_CONTAINER_H_

#include <memory>
#include <vector>

#include "ui/views/controls/button/label_button_image_container.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ui {
class ImageModel;
}  // namespace ui

namespace views {
class View;
class ImageView;
class LabelButton;
}  // namespace views

class MultiImageContainer final : public views::LabelButtonImageContainer {
 public:
  MultiImageContainer();
  MultiImageContainer(const MultiImageContainer&) = delete;
  MultiImageContainer& operator=(const MultiImageContainer&) = delete;
  ~MultiImageContainer() override;

  void SetImages(const std::vector<ui::ImageModel>& image_model);
  void SetImage(size_t index, const ui::ImageModel& image_model);
  void SetImage(size_t index, const gfx::ImageSkia& image);

  // LabelButtonImageContainer
  std::unique_ptr<views::View> CreateView() override;
  views::View* GetView() override;
  const views::View* GetView() const override;
  void UpdateImage(const views::LabelButton* button) override;

 private:
  void AddExtraImages(const size_t number_of_images);
  void RemoveExtraImages(const size_t number_of_images);

  std::vector<raw_ptr<views::ImageView>> images_;
  views::ViewTracker view_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_MULTI_IMAGE_CONTAINER_H_
