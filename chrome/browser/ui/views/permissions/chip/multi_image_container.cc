// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/chip/multi_image_container.h"

#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

MultiImageContainer::MultiImageContainer() = default;
MultiImageContainer::~MultiImageContainer() = default;

std::unique_ptr<views::View> MultiImageContainer::CreateView() {
  auto view_container = std::make_unique<views::View>();
  view_container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);
  view_container->SetCanProcessEventsWithinSubtree(false);

  auto* image =
      view_container->AddChildView(std::make_unique<views::ImageView>());
  image->SetCanProcessEventsWithinSubtree(false);
  images_.push_back(image);

  view_tracker_.SetView(view_container.get());
  return view_container;
}

void MultiImageContainer::SetImages(
    const std::vector<ui::ImageModel>& image_models) {
  if (images_.size() < image_models.size()) {
    AddExtraImages(image_models.size());
  } else if (images_.size() > image_models.size()) {
    RemoveExtraImages(image_models.size());
  }

  for (size_t i = 0; i < images_.size(); i++) {
    SetImage(i, image_models[i]);
  }
}

void MultiImageContainer::AddExtraImages(const size_t number_of_images) {
  if (auto* view = view_tracker_.view(); view) {
    for (size_t i = images_.size(); i < number_of_images; i++) {
      auto* image = view->AddChildView(std::make_unique<views::ImageView>());
      image->SetCanProcessEventsWithinSubtree(false);
      images_.push_back(image);
    }
  }
}

void MultiImageContainer::RemoveExtraImages(const size_t number_of_images) {
  if (auto* view = view_tracker_.view(); view) {
    for (size_t i = images_.size() - 1; i >= number_of_images; i--) {
      view->RemoveChildViewT(std::exchange(images_[i], nullptr));
    }
    images_.resize(number_of_images);
  }
}

void MultiImageContainer::SetImage(size_t index,
                                   const ui::ImageModel& image_model) {
  if (auto* view = view_tracker_.view(); view) {
    SetImage(index, image_model.Rasterize(view->GetColorProvider()));
  }
}

void MultiImageContainer::SetImage(size_t index, const gfx::ImageSkia& image) {
  if (index < images_.size()) {
    images_[index]->SetImage(ui::ImageModel::FromImageSkia(image));
  }
}

views::View* MultiImageContainer::GetView() {
  return view_tracker_.view();
}

const views::View* MultiImageContainer::GetView() const {
  return view_tracker_.view();
}

void MultiImageContainer::UpdateImage(const views::LabelButton* button) {
  // Update only the first image if exist.
  // This function is left as not to break existing behaviour till full
  // migration.
  SetImage(0, button->GetImage(button->GetVisualState()));
}
