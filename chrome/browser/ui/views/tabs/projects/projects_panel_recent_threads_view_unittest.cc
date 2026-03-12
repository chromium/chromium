// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

namespace {

// Helper to create a contextual_tasks::Thread.
contextual_tasks::Thread CreateThread(
    const std::string& title,
    std::optional<const std::string> server_id = std::nullopt) {
  return contextual_tasks::Thread(contextual_tasks::ThreadType::kAiMode,
                                  server_id.value_or(""), title,
                                  /*last_turn_time_unix_epoch_millis=*/1,
                                  /*conversation_turn_id=*/"");
}

const contextual_tasks::Thread& GetThread1() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 1", "id1"));
  return *thread;
}

const contextual_tasks::Thread& GetThread2() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 2", "id2"));
  return *thread;
}

const contextual_tasks::Thread& GetThread3() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 3", "id3"));
  return *thread;
}

const contextual_tasks::Thread& GetThread4() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 4", "id4"));
  return *thread;
}

const std::vector<contextual_tasks::Thread>& GetManyThreads() {
  static const base::NoDestructor<std::vector<contextual_tasks::Thread>>
      threads({GetThread1(), GetThread2(), GetThread3(), GetThread4()});
  return *threads;
}

MATCHER_P(IsForThread, expected_thread, "") {
  const views::Label* label = arg->title_for_testing();
  return base::UTF8ToUTF16(expected_thread.title) == label->GetText();
}

}  // namespace

class ProjectsPanelRecentThreadsViewTest : public views::ViewsTestBase {
 public:
  ProjectsPanelRecentThreadsViewTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(ProjectsPanelRecentThreadsViewTest, EmptyThreadsList) {
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();

  EXPECT_EQ(0u, recent_threads_view->children().size());
}

TEST_F(ProjectsPanelRecentThreadsViewTest, PopulatesThreadsList) {
  const std::vector<contextual_tasks::Thread> threads = {GetThread1(),
                                                         GetThread2()};
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();
  recent_threads_view->SetThreads(threads);

  EXPECT_EQ(2u, recent_threads_view->children().size());

  // Verify that the labels are set correctly.
  for (size_t i = 0; i < threads.size(); ++i) {
    auto* thread_item_view = recent_threads_view->item_views_for_testing()[i];
    EXPECT_THAT(thread_item_view, IsForThread(threads[i]));
  }
}

TEST_F(ProjectsPanelRecentThreadsViewTest, SetThreadsUpdatesList) {
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();
  recent_threads_view->SetThreads(
      std::vector<contextual_tasks::Thread>({GetThread1()}));

  EXPECT_EQ(1u, recent_threads_view->children().size());

  const std::vector<contextual_tasks::Thread> new_threads = {GetThread1(),
                                                             GetThread3()};
  recent_threads_view->SetThreads(new_threads);

  EXPECT_EQ(2u, recent_threads_view->children().size());

  for (size_t i = 0; i < new_threads.size(); ++i) {
    auto* thread_item_view = recent_threads_view->item_views_for_testing()[i];
    EXPECT_THAT(thread_item_view, IsForThread(new_threads[i]));
  }
}

TEST_F(ProjectsPanelRecentThreadsViewTest, PropagatesCallbackToItems) {
  const std::vector<contextual_tasks::Thread> threads = {GetThread1()};
  base::MockCallback<ProjectsPanelRecentThreadsView::ThreadPressedCallback>
      mock_callback;
  auto recent_threads_view =
      std::make_unique<ProjectsPanelRecentThreadsView>(mock_callback.Get());
  recent_threads_view->SetThreads(threads);

  EXPECT_CALL(mock_callback, Run(GetThread1().server_id, GetThread1().type))
      .Times(1);

  auto* thread_item_view = recent_threads_view->item_views_for_testing()[0];
  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  views::test::ButtonTestApi(thread_item_view).NotifyClick(event);
}

TEST_F(ProjectsPanelRecentThreadsViewTest, DisplaysUpToMaxNumberOfThreads) {
  std::vector<contextual_tasks::Thread> threads;
  for (size_t i = 0; i < projects_panel::kMaxNumberOfRecentThreads + 50; i++) {
    threads.emplace_back(CreateThread("Thread"));
  }

  // Create the view with more than the max number of threads.
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();
  recent_threads_view->SetThreads(threads);

  recent_threads_view->SetExpanded(true);

  // Ensure only the max number of threads are shown.
  EXPECT_EQ(projects_panel::kMaxNumberOfRecentThreads,
            recent_threads_view->item_views_for_testing().size());
}

TEST_F(ProjectsPanelRecentThreadsViewTest, CollapsesThreadsInitially) {
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();
  recent_threads_view->SetThreads(GetManyThreads());

  // Only 3 thread should be visible.
  EXPECT_EQ(3u, recent_threads_view->children().size());
  EXPECT_EQ(3u, recent_threads_view->item_views_for_testing().size());
}

TEST_F(ProjectsPanelRecentThreadsViewTest, ExpandList) {
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();
  recent_threads_view->SetThreads(GetManyThreads());

  recent_threads_view->SetExpanded(true);

  // All 4 threads should be visible after expanding.
  EXPECT_EQ(4u, recent_threads_view->children().size());
  EXPECT_EQ(4u, recent_threads_view->item_views_for_testing().size());
  EXPECT_TRUE(recent_threads_view->expanded());
}

TEST_F(ProjectsPanelRecentThreadsViewTest, CollapseList) {
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>();
  recent_threads_view->SetThreads(GetManyThreads());
  recent_threads_view->SetExpanded(true);

  EXPECT_EQ(4u, recent_threads_view->children().size());

  recent_threads_view->SetExpanded(false);

  // Only 3 threads should be visible after collapsing.
  EXPECT_EQ(3u, recent_threads_view->children().size());
  EXPECT_EQ(3u, recent_threads_view->item_views_for_testing().size());
  EXPECT_FALSE(recent_threads_view->expanded());
}
