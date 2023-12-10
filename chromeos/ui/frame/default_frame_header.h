// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_DEFAULT_FRAME_HEADER_H_
#define CHROMEOS_UI_FRAME_DEFAULT_FRAME_HEADER_H_

#include <memory>

#include "base/compiler_specific.h"  // override
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/frame/frame_color_metrics_helper.h"
#include "chromeos/ui/frame/frame_header.h"

namespace ash {
FORWARD_DECLARE_TEST(DefaultFrameHeaderTest, FrameColors);
FORWARD_DECLARE_TEST(NonClientFrameViewAshFrameColorTest,
                     WideFrameInitialColor);
}  // namespace ash

namespace chromeos {

// Helper class for managing the default window header, which is used for
// Chrome apps (but not bookmark apps), for example.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) DefaultFrameHeader
    : public FrameHeader {
 public:
  // DefaultFrameHeader does not take ownership of any of the parameters.
  DefaultFrameHeader(
      views::Widget* target_widget,
      views::View* header_view,
      chromeos::FrameCaptionButtonContainerView* caption_button_container);

  DefaultFrameHeader(const DefaultFrameHeader&) = delete;
  DefaultFrameHeader& operator=(const DefaultFrameHeader&) = delete;

  ~DefaultFrameHeader() override;

  void SetWidthInPixels(int width_in_pixels);

  // FrameHeader:
  void UpdateFrameColors() override;

 protected:
  // FrameHeader:
  void DoPaintHeader(gfx::Canvas* canvas) override;
  views::CaptionButtonLayoutSize GetButtonLayoutSize() const override;
  SkColor GetTitleColor() const override;
  SkColor GetCurrentFrameColor() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::DefaultFrameHeaderTest, FrameColors);
  FRIEND_TEST_ALL_PREFIXES(ash::NonClientFrameViewAshFrameColorTest,
                           WideFrameInitialColor);

  // Returns the window of the target widget.
  aura::Window* GetTargetWindow();

  // This function should be only called once, i.e., one instance of
  // default_frame_header should own only one `frame_color_metrics_helper_`
  // throughout its lifetime.
  void InitializeFrameColorMetricsHelper();

  SkColor active_frame_color_ = chromeos::kDefaultFrameColor;
  SkColor inactive_frame_color_ = chromeos::kDefaultFrameColor;

  int width_in_pixels_ = -1;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<FrameColorMetricsHelper> frame_color_metrics_helper_;
#endif
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_DEFAULT_FRAME_HEADER_H_
