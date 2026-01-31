// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_recent_threads_view.h"

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

namespace {

// Helper to create a contextual_tasks::Thread.
contextual_tasks::Thread CreateThread(const std::string& title) {
  return contextual_tasks::Thread(contextual_tasks::ThreadType::kAiMode,
                                  /*server_id=*/"", title,
                                  /*conversation_turn_id=*/"");
}

const contextual_tasks::Thread& GetThread1() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 1"));
  return *thread;
}

const contextual_tasks::Thread& GetThread2() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 2"));
  return *thread;
}

const contextual_tasks::Thread& GetThread3() {
  static const base::NoDestructor<contextual_tasks::Thread> thread(
      CreateThread("Thread 3"));
  return *thread;
}

MATCHER_P(IsForThread, expected_thread, "") {
  auto* label = static_cast<views::Label*>(arg->children()[2]);
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
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>(
      std::vector<contextual_tasks::Thread>());

  EXPECT_EQ(0u, recent_threads_view->children().size());
}

TEST_F(ProjectsPanelRecentThreadsViewTest, PopulatesThreadsList) {
  const std::vector<contextual_tasks::Thread> threads = {GetThread1(),
                                                         GetThread2()};
  auto recent_threads_view =
      std::make_unique<ProjectsPanelRecentThreadsView>(threads);

  EXPECT_EQ(2u, recent_threads_view->children().size());

  // Verify that the labels are set correctly.
  for (size_t i = 0; i < threads.size(); ++i) {
    auto* thread_item_view = static_cast<ProjectsPanelThreadItemView*>(
        recent_threads_view->children()[i]);
    EXPECT_THAT(thread_item_view, IsForThread(threads[i]));
  }
}

TEST_F(ProjectsPanelRecentThreadsViewTest, SetThreadsUpdatesList) {
  auto recent_threads_view = std::make_unique<ProjectsPanelRecentThreadsView>(
      std::vector<contextual_tasks::Thread>({GetThread1()}));

  EXPECT_EQ(1u, recent_threads_view->children().size());

  const std::vector<contextual_tasks::Thread> new_threads = {GetThread1(),
                                                             GetThread3()};
  recent_threads_view->SetThreads(new_threads);

  EXPECT_EQ(2u, recent_threads_view->children().size());

  for (size_t i = 0; i < new_threads.size(); ++i) {
    auto* thread_item_view = static_cast<ProjectsPanelThreadItemView*>(
        recent_threads_view->children()[i]);
    EXPECT_THAT(thread_item_view, IsForThread(new_threads[i]));
  }
}
