// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/ash/input_method/candidate_view.h"

#include <stddef.h>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace ui {
namespace ime {
namespace {

const char* const kDummyCandidates[] = {
    "candidate1",
    "candidate2",
    "candidate3",
};

}  // namespace

class CandidateViewTest : public views::ViewsTestBase {
 public:
  CandidateViewTest() = default;

  CandidateViewTest(const CandidateViewTest&) = delete;
  CandidateViewTest& operator=(const CandidateViewTest&) = delete;

  ~CandidateViewTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    views::Widget::InitParams init_params(
        CreateParams(views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW));

    init_params.delegate = new views::WidgetDelegateView();

    container_ = init_params.delegate->GetContentsView();
    container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    for (size_t i = 0; i < std::size(kDummyCandidates); ++i) {
      CandidateView* candidate = new CandidateView(
          views::Button::PressedCallback(), ui::CandidateWindow::VERTICAL);
      ui::CandidateWindow::Entry entry;
      entry.value = base::UTF8ToUTF16(kDummyCandidates[i]);
      candidate->SetEntry(entry);
      container_->AddChildView(candidate);
    }

    widget_ = new views::Widget();
    widget_->Init(std::move(init_params));
    widget_->Show();

    aura::Window* native_window = widget_->GetNativeWindow();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        native_window->GetRootWindow(), native_window);
  }

  void TearDown() override {
    widget_->Close();

    views::ViewsTestBase::TearDown();
  }

 protected:
  CandidateView* GetCandidateAt(size_t index) {
    return static_cast<CandidateView*>(container_->children()[index]);
  }

  size_t GetHighlightedCount() const {
    const auto& children = container_->children();
    return base::ranges::count_if(
        children, [](const views::View* v) { return !!v->background(); });
  }

  int GetHighlightedIndex() const {
    const auto& children = container_->children();
    const auto it = base::ranges::find_if(
        children, [](const views::View* v) { return !!v->background(); });
    return (it == children.cend()) ? -1 : std::distance(children.cbegin(), it);
  }

  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }

 private:
  raw_ptr<views::Widget, DanglingUntriaged> widget_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> container_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

TEST_F(CandidateViewTest, MouseHovers) {
  GetCandidateAt(0)->SetHighlighted(true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  // Mouse hover shouldn't change the background.
  event_generator()->MoveMouseTo(
      GetCandidateAt(0)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  // Mouse hover shouldn't change the background.
  event_generator()->MoveMouseTo(
      GetCandidateAt(1)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  // Mouse hover shouldn't change the background.
  event_generator()->MoveMouseTo(
      GetCandidateAt(2)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());
}

TEST_F(CandidateViewTest, MouseClick) {
  bool clicked = false;
  CandidateView* view = GetCandidateAt(1);
  view->SetCallback(
      base::BindRepeating([](bool* clicked) { *clicked = true; }, &clicked));
  event_generator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator()->ClickLeftButton();
  EXPECT_TRUE(clicked);
}

TEST_F(CandidateViewTest, ClickAndMove) {
  GetCandidateAt(0)->SetHighlighted(true);

  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  bool clicked = false;
  CandidateView* view = GetCandidateAt(1);
  view->SetCallback(
      base::BindRepeating([](bool* clicked) { *clicked = true; }, &clicked));
  event_generator()->MoveMouseTo(
      GetCandidateAt(2)->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(2, GetHighlightedIndex());

  // Highlight follows the drag.
  event_generator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(1, GetHighlightedIndex());

  event_generator()->MoveMouseTo(
      GetCandidateAt(0)->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(0, GetHighlightedIndex());

  event_generator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(1u, GetHighlightedCount());
  EXPECT_EQ(1, GetHighlightedIndex());

  EXPECT_FALSE(clicked);
  event_generator()->ReleaseLeftButton();
  EXPECT_TRUE(clicked);
}

TEST_F(CandidateViewTest, SetEntryChangesAccessibleName) {
  CandidateView* view = GetCandidateAt(1);

  ui::CandidateWindow::Entry entry;
  entry.value = u"Candidate";
  view->SetEntry(entry);
  EXPECT_EQ(u"Candidate", view->GetViewAccessibility().GetCachedName());
}

TEST_F(CandidateViewTest, SetEntryNotifiesAccessibilityEvent) {
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  CandidateView* view = GetCandidateAt(1);

  // Calling SetEntry affects the accessible name, so it should notify twice:
  // once for CandidateView's child label and once for CandidateView itself.
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged));
  ui::CandidateWindow::Entry entry;
  entry.value = u"Candidate";
  view->SetEntry(entry);
  EXPECT_EQ(2, counter.GetCount(ax::mojom::Event::kTextChanged));
}

TEST_F(CandidateViewTest, AccessibilityAttributes) {
  CandidateView* view = GetCandidateAt(1);

  ui::CandidateWindow::Entry entry;
  entry.value = u"Candidate";
  view->SetEntry(entry);

  ui::AXNodeData data;
  static_cast<views::View*>(view)->GetViewAccessibility().GetAccessibleNodeData(
      &data);

  EXPECT_EQ(ax::mojom::Role::kImeCandidate, data.role);
  EXPECT_EQ("Candidate",
            data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(static_cast<int>(ax::mojom::DefaultActionVerb::kPress),
            data.GetIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));
  EXPECT_EQ(0, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(0, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));

  view->SetPositionData(1, 3);
  data = ui::AXNodeData();
  static_cast<views::View*>(view)->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(2, data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet));
  EXPECT_EQ(3, data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

}  // namespace ime
}  // namespace ui
