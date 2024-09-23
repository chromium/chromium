// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/shortcut_integration_interaction_test_internal.h"

#include <map>
#include <set>

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
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

  void MaybeTriggerShownEvent() {
    if (did_trigger_shown_) {
      return;
    }
    {
      // Postpone dispatching the "shown" event for this shortcut until the
      // file contains at least some data. Creating the file and writing its
      // contents is not an atomic operation, so sometimes an empty file can be
      // observed.
      base::ScopedAllowBlockingForTesting allow_io;
      int64_t file_size = 0;
      if (!base::GetFileSize(path_, &file_size) || file_size == 0) {
        // If the file isn't already being watched, start watching it. We can't
        // just piggy-back of the FilePathWatcher in ShortcutTracker, as on
        // macOS FilePathWatcher on a directory does not monitor changes to
        // files in that directory.
        if (!path_watcher_) {
          path_watcher_.emplace(
              base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
          path_watcher_.AsyncCall(&base::FilePathWatcher::Watch)
              .WithArgs(path_, base::FilePathWatcher::Type::kNonRecursive,
                        base::BindPostTaskToCurrentDefault(base::BindRepeating(
                            &TrackedShortcut::ContentChanged,
                            weak_ptr_factory_.GetWeakPtr())))
              // Re-check if the shortcut has been written to once the
              // FilePathWatcher has been set-up, to catch any cases where the
              // file is written to after we originally check its contents but
              // before the FilePathWatcher is in place.
              .Then(base::BindOnce([](bool result) { EXPECT_TRUE(result); })
                        .Then(base::BindOnce(
                            &TrackedShortcut::MaybeTriggerShownEvent,
                            weak_ptr_factory_.GetWeakPtr())));
        }
        return;
      }
    }

    did_trigger_shown_ = true;
    // No longer need to watch for file changes. Deletion of the shortcut is
    // handled in ShortcutTracker.
    path_watcher_.Reset();
    ui::ElementTracker::GetFrameworkDelegate()->NotifyElementShown(this);
  }

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
 private:
  void ContentChanged(const base::FilePath& path, bool error) {
    EXPECT_FALSE(error);
    MaybeTriggerShownEvent();
  }

  base::FilePath path_;

  // Set to true when ui::ElementTracker has been notified that this shortcut
  // has been shown. This is done to avoid notifying twice in cases where
  // MaybeTriggerShownEvent gets called multiple times.
  bool did_trigger_shown_ = false;
  base::SequenceBound<base::FilePathWatcher> path_watcher_;

  base::WeakPtrFactory<TrackedShortcut> weak_ptr_factory_{this};
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TrackedShortcut)

}  // namespace

// This class monitors a specified directory, creating (and destroying)
// `TrackedShortcut` instances for any files created and removed from the
// directory being monitored. This makes it possible to treat files in the given
// directory as elements using the interactive test framework.
class ShortcutIntegrationInteractionTestPrivate::ShortcutTracker {
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
    // Now maybe notify `ElementTracker` of newly observed and tracked files.
    // This is done last to avoid any re-entrency issues for actions that are
    // triggered by this.
    for (TrackedShortcut* new_shortcut : new_shortcuts) {
      new_shortcut->MaybeTriggerShownEvent();
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

ShortcutIntegrationInteractionTestPrivate
    ::ShortcutIntegrationInteractionTestPrivate()
    : internal::InteractiveBrowserTestPrivate(
          std::make_unique<InteractionTestUtilBrowser>()) {}

ShortcutIntegrationInteractionTestPrivate::
    ~ShortcutIntegrationInteractionTestPrivate() = default;

void ShortcutIntegrationInteractionTestPrivate::DoTestSetUp() {
  internal::InteractiveBrowserTestPrivate::DoTestSetUp();
  test_support_ = std::make_unique<ShortcutCreationTestSupport>();
  shortcut_tracker_ = std::make_unique<ShortcutTracker>(
      base::PathService::CheckedGet(base::DIR_USER_DESKTOP));
}

void ShortcutIntegrationInteractionTestPrivate::DoTestTearDown() {
  shortcut_tracker_.reset();
  test_support_.reset();
  internal::InteractiveBrowserTestPrivate::DoTestTearDown();
}

void ShortcutIntegrationInteractionTestPrivate::SetNextShortcutIdentifier(
    ui::ElementIdentifier identifier) {
  shortcut_tracker_->SetNextShortcutIdentifier(identifier);
}

// static
base::FilePath ShortcutIntegrationInteractionTestPrivate::GetShortcutPath(
    ui::TrackedElement* element) {
  CHECK(element->IsA<TrackedShortcut>());
  TrackedShortcut* const shortcut = element->AsA<TrackedShortcut>();
  return shortcut->path();
}

}  // namespace shortcuts
