// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/observer_list.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/animation/test_animation_delegate.h"

class TestStackView : public views::View {
 public:
  class Observer {
   public:
    Observer() : view_deleted_(false) {}

    void OnViewBeingDeleted() {
      view_deleted_ = true;
    }

    bool view_deleted() { return view_deleted_; }

   private:
    bool view_deleted_;
  };

  TestStackView() {}

  TestStackView(const TestStackView&) = delete;
  TestStackView& operator=(const TestStackView&) = delete;

  ~TestStackView() override {
    observers_.Notify(&Observer::OnViewBeingDeleted);
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

class ViewStackTest : public ChromeViewsTestBase {
 public:
  ViewStackTest() : view_stack_(std::make_unique<ViewStack>()) {
    view_stack_->SetBounds(0, 0, 10, 10);
    view_stack_->Push(std::make_unique<TestStackView>(), false);
    view_stack_->slide_in_animator_->SetAnimationDuration(
        base::Milliseconds(1));
    view_stack_->slide_out_animator_->SetAnimationDuration(
        base::Milliseconds(1));
  }

  ViewStackTest(const ViewStackTest&) = delete;
  ViewStackTest& operator=(const ViewStackTest&) = delete;

  void AssertViewOnTopOfStack(views::View* view) {
    gfx::Rect target = view_stack_->bounds();
    target.set_origin(gfx::Point(0, 0));
    EXPECT_EQ(target, view->bounds());
  }

  void AssertViewCompletelyNextToStack(views::View* view) {
    gfx::Rect target = view_stack_->bounds();
    target.set_origin(gfx::Point(view_stack_->width(), 0));
    EXPECT_EQ(target, view->bounds());
  }

  // Pushes a view on the stack, waits for its animation to be over, then
  // returns a pointer to the pushed view.
  views::View* PushViewOnStackAndWait() {
    base::RunLoop loop;
    std::unique_ptr<TestStackView> view = std::make_unique<TestStackView>();
    views::View* view_ptr = view.get();

    view_stack_->Push(std::move(view), true);
    EXPECT_TRUE(view_stack_->slide_in_animator_->IsAnimating());
    view_stack_->slide_in_animator_->SetAnimationDelegate(
        view_ptr,
        std::unique_ptr<gfx::AnimationDelegate>(
            new gfx::TestAnimationDelegate(loop.QuitWhenIdleClosure())));

    loop.Run();
    EXPECT_FALSE(view_stack_->slide_in_animator_->IsAnimating());
    return view_ptr;
  }

  // Pops |n| views from the stack, then waits for |top_view_ptr|'s animation to
  // be over.
  void PopManyAndWait(int n, views::View* top_view_ptr) {
    base::RunLoop loop;
    view_stack_->PopMany(n);

    EXPECT_TRUE(view_stack_->slide_out_animator_->IsAnimating());
    view_stack_->slide_out_animator_->SetAnimationDelegate(
        top_view_ptr,
        std::unique_ptr<gfx::AnimationDelegate>(
            new gfx::TestAnimationDelegate(loop.QuitWhenIdleClosure())));

    loop.Run();
    EXPECT_FALSE(view_stack_->slide_out_animator_->IsAnimating());
  }

  std::unique_ptr<ViewStack> view_stack_;
};

TEST_F(ViewStackTest, TestInitialStateAddedAsChildView) {
  EXPECT_EQ(1u, view_stack_->children().size());
  // This child was added without any animation so it's on top of its parent
  // already.
  AssertViewOnTopOfStack(view_stack_->top());
}

TEST_F(ViewStackTest, TestPushStateAddsViewToChildren) {
  view_stack_->Push(std::make_unique<TestStackView>(), true);
  EXPECT_EQ(2u, view_stack_->children().size());

  AssertViewCompletelyNextToStack(view_stack_->top());
}

TEST_F(ViewStackTest, TestPopStateRemovesChildViewAndCleansUpState) {
  base::RunLoop loop1;
  base::RunLoop loop2;
  TestStackView::Observer observer;
  std::unique_ptr<TestStackView> view = std::make_unique<TestStackView>();
  view->AddObserver(&observer);
  views::View* view_ptr = view.get();

  view_stack_->Push(std::move(view), true);
  EXPECT_TRUE(view_stack_->slide_in_animator_->IsAnimating());
  view_stack_->slide_in_animator_->SetAnimationDelegate(
      view_ptr,
      std::unique_ptr<gfx::AnimationDelegate>(
          new gfx::TestAnimationDelegate(loop1.QuitWhenIdleClosure())));

  AssertViewCompletelyNextToStack(view_ptr);

  loop1.Run();
  AssertViewOnTopOfStack(view_ptr);
  EXPECT_FALSE(view_stack_->slide_in_animator_->IsAnimating());
  view_stack_->Pop();

  EXPECT_TRUE(view_stack_->slide_out_animator_->IsAnimating());
  view_stack_->slide_out_animator_->SetAnimationDelegate(
      view_ptr,
      std::unique_ptr<gfx::AnimationDelegate>(
          new gfx::TestAnimationDelegate(loop2.QuitWhenIdleClosure())));

  loop2.Run();
  EXPECT_FALSE(view_stack_->slide_out_animator_->IsAnimating());

  ASSERT_TRUE(observer.view_deleted());
}

TEST_F(ViewStackTest, TestDeletingViewCleansUpState) {
  TestStackView::Observer observer;
  base::RunLoop loop;
  std::unique_ptr<TestStackView> view = std::make_unique<TestStackView>();
  view->AddObserver(&observer);
  views::View* view_ptr = view.get();

  view_stack_->Push(std::move(view), true);
  EXPECT_TRUE(view_stack_->slide_in_animator_->IsAnimating());
  view_stack_->slide_in_animator_->SetAnimationDelegate(
      view_ptr,
      std::unique_ptr<gfx::AnimationDelegate>(
          new gfx::TestAnimationDelegate(loop.QuitWhenIdleClosure())));

  AssertViewCompletelyNextToStack(view_ptr);

  loop.Run();
  AssertViewOnTopOfStack(view_ptr);
  EXPECT_FALSE(view_stack_->slide_in_animator_->IsAnimating());
  view_stack_->Pop();

  EXPECT_TRUE(view_stack_->slide_out_animator_->IsAnimating());
  view_stack_.reset();

  ASSERT_TRUE(observer.view_deleted());
}

TEST_F(ViewStackTest, TestLayoutUpdatesAnimations) {
  TestStackView::Observer observer;
  base::RunLoop loop1;
  base::RunLoop loop2;
  std::unique_ptr<TestStackView> view = std::make_unique<TestStackView>();
  view->AddObserver(&observer);
  views::View* view_ptr = view.get();

  view_stack_->Push(std::move(view), true);
  EXPECT_TRUE(view_stack_->slide_in_animator_->IsAnimating());
  view_stack_->slide_in_animator_->SetAnimationDelegate(
      view_ptr,
      std::unique_ptr<gfx::AnimationDelegate>(
          new gfx::TestAnimationDelegate(loop1.QuitWhenIdleClosure())));

  view_stack_->SetBounds(10, 10, 30, 30);

  loop1.Run();
  AssertViewOnTopOfStack(view_ptr);
  EXPECT_FALSE(view_stack_->slide_in_animator_->IsAnimating());
  view_stack_->Pop();

  EXPECT_TRUE(view_stack_->slide_out_animator_->IsAnimating());
  view_stack_->slide_out_animator_->SetAnimationDelegate(
      view_ptr,
      std::unique_ptr<gfx::AnimationDelegate>(
          new gfx::TestAnimationDelegate(loop2.QuitWhenIdleClosure())));

  loop2.Run();
  EXPECT_FALSE(view_stack_->slide_out_animator_->IsAnimating());

  ASSERT_TRUE(observer.view_deleted());
}

TEST_F(ViewStackTest, TestPopMany) {
  views::View* top = PushViewOnStackAndWait();
  EXPECT_EQ(2U, view_stack_->GetSize());

  PopManyAndWait(1, top);
  EXPECT_EQ(1U, view_stack_->GetSize());

  top = PushViewOnStackAndWait();
  top = PushViewOnStackAndWait();
  top = PushViewOnStackAndWait();
  EXPECT_EQ(4U, view_stack_->GetSize());

  PopManyAndWait(3, top);
  EXPECT_EQ(1U, view_stack_->GetSize());

  top = PushViewOnStackAndWait();
  top = PushViewOnStackAndWait();
  EXPECT_EQ(3U, view_stack_->GetSize());
}
