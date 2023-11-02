// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/views/layer_dimmer.h"
#include <memory>

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

namespace javascript_dialogs {

class LayerDimmerTest : public testing::Test {
 protected:
  void SetUp() override {
    parentWindow_ = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    contentWindow_ = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    dialogWindow_ = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_POPUP);

    // Set up the windows' layers
    parentWindow_->Init(ui::LAYER_NOT_DRAWN);
    parentWindow_->SetBounds(gfx::Rect(1000, 500));

    contentWindow_->Init(ui::LAYER_TEXTURED);
    contentWindow_->SetBounds(gfx::Rect(1000, 500));
    parentWindow_->AddChild(contentWindow_.get());

    dialogWindow_->Init(ui::LAYER_TEXTURED);
    dialogWindow_->SetBounds(gfx::Rect(400, 100));
    parentWindow_->AddChild(dialogWindow_.get());

    layerDimmer_ =
        std::make_unique<LayerDimmer>(parentWindow_.get(), dialogWindow_.get());
  }

  std::unique_ptr<aura::Window> parentWindow_;
  std::unique_ptr<aura::Window> contentWindow_;
  std::unique_ptr<aura::Window> dialogWindow_;
  std::unique_ptr<LayerDimmer> layerDimmer_;
};

TEST_F(LayerDimmerTest, TestBoundsChange) {
  gfx::Rect parentBounds = parentWindow_->layer()->bounds();
  gfx::Rect dimmerBounds = layerDimmer_->GetLayerForTest()->bounds();
  EXPECT_TRUE(dimmerBounds.ApproximatelyEqual(parentBounds, 0));

  // Simulate a resize
  const gfx::Rect newBounds(950, 470);
  parentWindow_->SetBounds(newBounds);

  parentBounds = parentWindow_->layer()->bounds();
  dimmerBounds = layerDimmer_->GetLayerForTest()->bounds();
  EXPECT_TRUE(dimmerBounds.ApproximatelyEqual(newBounds, 0));
}

TEST_F(LayerDimmerTest, TestShowHide) {
  EXPECT_FLOAT_EQ(layerDimmer_->GetLayerForTest()->GetTargetOpacity(), 0.f);

  layerDimmer_->Show();
  EXPECT_FLOAT_EQ(layerDimmer_->GetLayerForTest()->GetTargetOpacity(), 1.f);

  layerDimmer_->Hide();
  EXPECT_FLOAT_EQ(layerDimmer_->GetLayerForTest()->GetTargetOpacity(), 0.f);
}

TEST_F(LayerDimmerTest, TestLayerOrder) {
  // Layer order should be correct after creating the LayerDimmer.
  // (The last child is on top)
  auto childLayers = parentWindow_->layer()->children();
  EXPECT_THAT(childLayers, testing::ElementsAre(contentWindow_->layer(),
                                                layerDimmer_->GetLayerForTest(),
                                                dialogWindow_->layer()));

  // Simulate stacking change which could re-order the layers. This can happen
  // when the user clicks on the dialog window.
  parentWindow_->layer()->StackAtBottom(layerDimmer_->GetLayerForTest());
  layerDimmer_->OnWindowStackingChanged(dialogWindow_.get());

  // Verify order is still the same
  childLayers = parentWindow_->layer()->children();
  EXPECT_THAT(childLayers, testing::ElementsAre(contentWindow_->layer(),
                                                layerDimmer_->GetLayerForTest(),
                                                dialogWindow_->layer()));
}

}  // namespace javascript_dialogs
