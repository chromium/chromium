// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/shortcut_integration_browsertest_internal.h"

#include <map>
#include <set>

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/test_future.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/shortcuts/shortcut_creation_test_support.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"

namespace shortcuts {

namespace {

// ui::TrackedElement implementation that represents a file on disk, typically a
// file representing a shortcut created by the "create shortcut" flow, although
// nothing in this class is limited to such files.
class TrackedShortcut : public ui::TrackedElement {
 public:
  TrackedShortcut(ui::ElementIdentifier identifier,
                  ui::ElementContext context,
                  const base::FilePath& path)
      : ui::TrackedElement(identifier, context), path_(path) {}

  std::string ToString() const override {
    return base::StrCat(
        {ui::TrackedElement::ToString(), " path=", path_.AsUTF8Unsafe()});
  }

  const base::FilePath& path() const { return path_; }

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

 private:
  base::FilePath path_;
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedShortcut)

}  // namespace

// This class monitors a specified directory, creating (and destroying)
// `TrackedShortcut` instances for any files created and removed from the
// directory being monitored. This makes it possible to treat files in the given
// directory as elements using the interactive test framework.
class ShortcutIntegrationBrowserTestPrivate::ShortcutTracker {
 public:
  explicit ShortcutTracker(const base::FilePath& path)
      : path_(path),
        path_watcher_(
            base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
    base::test::TestFuture<bool> watch_result;
    path_watcher_.AsyncCall(&base::FilePathWatcher::Watch)
        .WithArgs(path, base::FilePathWatcher::Type::kNonRecursive,
                  base::BindPostTaskToCurrentDefault(
                      base::BindRepeating(&ShortcutTracker::ContentChanged,
                                          weak_ptr_factory_.GetWeakPtr())))
        .Then(watch_result.GetCallback());
    EXPECT_TRUE(watch_result.Get());
    UpdateTrackedShortcuts();
  }

  // Associates the next newly created file to be detected in the monitored
  // directory with `identifier`. For now we only support one pending "next"
  // shortcut. This shortcut needs to be created (and detected as having been
  // created) before another "next" shortcut can be identified.
  void SetNextShortcutIdentifier(ui::ElementIdentifier identifier) {
    CHECK(identifier);
    CHECK(!next_shortcut_identifier_);
    next_shortcut_identifier_ = identifier;
  }

 private:
  void ContentChanged(const base::FilePath& path, bool error) {
    EXPECT_FALSE(error);
    UpdateTrackedShortcuts();
  }

  void UpdateTrackedShortcuts() {
    // Gather all the paths that currently exist in the directory being
    // monitored.
    std::set<base::FilePath> current_paths;
    {
      base::ScopedAllowBlockingForTesting allow_io;
      base::FileEnumerator e(path_, /*recursive=*/false,
                             base::FileEnumerator::FILES);
      for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
        // Ignore hidden files (i.e. files whose name starts with '.').
        if (name.BaseName().value()[0] == base::FilePath::kExtensionSeparator) {
          continue;
        }
        current_paths.insert(name);
      }
    }
    // Add all new paths to `shortcuts_`, and if `next_shortcut_identifier_` was
    // set, associate the first new path with that identifier by creating a
    // `TrackedShortcut` instance.
    std::vector<TrackedShortcut*> new_shortcuts;
    for (const base::FilePath& path : current_paths) {
      if (base::Contains(shortcuts_, path)) {
        continue;
      }
      std::unique_ptr<TrackedShortcut> shortcut;
      if (!next_shortcut_identifier_) {
        LOG(INFO) << "Found new shortcut " << path
                  << " while not expecting new files.";
      } else {
        shortcut = std::make_unique<TrackedShortcut>(
            next_shortcut_identifier_, ui::ElementContext(this), path);
      }
      const auto [it, inserted] = shortcuts_.emplace(path, std::move(shortcut));
      next_shortcut_identifier_ = {};
      CHECK(inserted);
      if (it->second) {
        new_shortcuts.push_back(it->second.get());
      }
    }
    // Remove any paths from `shortcuts_` that no longer exist, notifying
    // `ElementTracker` of any that were tracked.
    std::erase_if(shortcuts_, [&](const auto& item) {
      bool should_erase = !base::Contains(current_paths, item.first);
      if (should_erase) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyElementHidden(
            item.second.get());
      }
      return should_erase;
    });
    // Now notify `ElementTracker` of newly observed and tracked files. This is
    // done last to avoid any re-entrency issues for actions that are triggered
    // by this.
    // TODO(https://crbug.com/343247628): Delay this until the shortcuts have
    // been fully written to disk, not just created as possibly empty files.
    for (TrackedShortcut* new_shortcut : new_shortcuts) {
      ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(
          new_shortcut);
    }
  }

  // The path of the directory being monitored.
  const base::FilePath path_;
  base::SequenceBound<base::FilePathWatcher> path_watcher_;

  // Map containing all the paths in `path_` this class is aware off, as well as
  // associated TrackedShortcut instanced. If a path is not one of interest
  // (i.e. does not have an associated ui::ElementIdentifier), the associated
  // value is null.
  std::map<base::FilePath, std::unique_ptr<TrackedShortcut>> shortcuts_;

  // ui::ElementIdentifier to associate with the next newly observed path in the
  // directory being monitored.
  ui::ElementIdentifier next_shortcut_identifier_;

  base::WeakPtrFactory<ShortcutTracker> weak_ptr_factory_{this};
};

ShortcutIntegrationBrowserTestPrivate::ShortcutIntegrationBrowserTestPrivate()
    : internal::InteractiveBrowserTestPrivate(
          std::make_unique<InteractionTestUtilBrowser>()) {}

ShortcutIntegrationBrowserTestPrivate::
    ~ShortcutIntegrationBrowserTestPrivate() = default;

void ShortcutIntegrationBrowserTestPrivate::DoTestSetUp() {
  internal::InteractiveBrowserTestPrivate::DoTestSetUp();
  test_support_ = std::make_unique<ShortcutCreationTestSupport>();
  shortcut_tracker_ = std::make_unique<ShortcutTracker>(
      base::PathService::CheckedGet(base::DIR_USER_DESKTOP));
}

void ShortcutIntegrationBrowserTestPrivate::DoTestTearDown() {
  shortcut_tracker_.reset();
  test_support_.reset();
  internal::InteractiveBrowserTestPrivate::DoTestTearDown();
}

void ShortcutIntegrationBrowserTestPrivate::SetNextShortcutIdentifier(
    ui::ElementIdentifier identifier) {
  shortcut_tracker_->SetNextShortcutIdentifier(identifier);
}

// static
base::FilePath ShortcutIntegrationBrowserTestPrivate::GetShortcutPath(
    ui::TrackedElement* element) {
  CHECK(element->IsA<TrackedShortcut>());
  TrackedShortcut* const shortcut = element->AsA<TrackedShortcut>();
  return shortcut->path();
}

}  // namespace shortcuts
