// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/scrollable_element.h"

#include "chrome/browser/vr/input_event.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace vr {

namespace {

std::unique_ptr<InputEvent> CreateScrollUpdate(float delta_x, float delta_y) {
  auto gesture = std::make_unique<InputEvent>(InputEvent::kScrollBegin);
  gesture->scroll_data.delta_x = delta_x;
  gesture->scroll_data.delta_y = delta_y;
  return gesture;
}

}  // namespace

TEST(ScrollableElement, VerticalOnScrollUpdate) {
  auto element =
      std::make_unique<ScrollableElement>(ScrollableElement::kVertical);
  element->set_max_span(1.0f);
  auto child = std::make_unique<UiElement>();
  child->SetSize(1.0f, 2.0f);
  element->AddScrollingChild(std::move(child));
  element->SizeAndLayOut();

  EXPECT_FLOAT_EQ(0.0f, element->scroll_offset());

  struct {
    float delta_x;
    float delta_y;
    float expected;
  } test_cases[] = {
      {1000.0f, 1000.0f, -0.5f}, {-1000.0f, -1000.0f, 0.5f},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    element->OnScrollUpdate(
        CreateScrollUpdate(test_cases[i].delta_x, test_cases[i].delta_y), {});
    EXPECT_FLOAT_EQ(test_cases[i].expected, element->scroll_offset());
  }
}

TEST(ScrollableElement, VerticalTopOnScrollUpdate) {
  auto element =
      std::make_unique<ScrollableElement>(ScrollableElement::kVertical);
  element->set_max_span(1.0f);
  element->SetScrollAnchoring(TOP);
  auto child = std::make_unique<UiElement>();
  child->SetSize(1.0f, 2.0f);
  element->AddScrollingChild(std::move(child));
  element->SizeAndLayOut();

  EXPECT_FLOAT_EQ(-0.5f, element->scroll_offset());

  struct {
    float delta_x;
    float delta_y;
    float expected;
  } test_cases[] = {{-1000.0f, -1000.0f, 0.5f}, {1000.0f, 1000.0f, -0.5f}};

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    element->OnScrollUpdate(
        CreateScrollUpdate(test_cases[i].delta_x, test_cases[i].delta_y), {});
    EXPECT_FLOAT_EQ(test_cases[i].expected, element->scroll_offset());
  }
}

TEST(ScrollableElement, HorizontalScrollUpdate) {
  auto element =
      std::make_unique<ScrollableElement>(ScrollableElement::kHorizontal);
  element->set_max_span(1.0f);
  auto child = std::make_unique<UiElement>();
  child->SetSize(2.0f, 1.0f);
  element->AddScrollingChild(std::move(child));
  element->SizeAndLayOut();

  EXPECT_FLOAT_EQ(0.0f, element->scroll_offset());

  struct {
    float delta_x;
    float delta_y;
    float expected;
  } test_cases[] = {
      {1000.0f, 1000.0f, -0.5f}, {-1000.0f, -1000.0f, 0.5f},
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    SCOPED_TRACE(i);
    element->OnScrollUpdate(
        CreateScrollUpdate(test_cases[i].delta_x, test_cases[i].delta_y), {});
    EXPECT_FLOAT_EQ(test_cases[i].expected, element->scroll_offset());
  }
}

TEST(ScrollableElement, MaxSpan) {
  auto element =
      std::make_unique<ScrollableElement>(ScrollableElement::kVertical);
  element->set_max_span(1.0f);
  EXPECT_FALSE(element->scrollable());

  // Add a child bigger than the maximum span.
  auto child = std::make_unique<UiElement>();
  child->SetSize(1.0f, 2.0f);
  element->AddScrollingChild(std::move(child));
  element->SizeAndLayOut();
  EXPECT_SIZEF_EQ(gfx::SizeF(1.0f, 1.0f), element->size());
  EXPECT_TRUE(element->scrollable());

  // Make the max span bigger so that it can fit the entire child.
  element->set_max_span(3.0f);
  element->SizeAndLayOut();
  EXPECT_SIZEF_EQ(gfx::SizeF(1.0f, 2.0f), element->size());
  EXPECT_FALSE(element->scrollable());
}

}  // namespace vr
