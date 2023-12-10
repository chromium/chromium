// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ash_test_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash::test {

namespace {

// Returns the path of the downloads mount point associated with the `profile`.
base::FilePath GetDownloadsPath(Profile* profile) {
  base::FilePath result;
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(profile), &result));
  return result;
}

aura::Window* GetRootWindow(const views::View* view) {
  return view->GetWidget()->GetNativeWindow()->GetRootWindow();
}

}  // namespace

void Click(const views::View* view, int flags) {
  ui::test::EventGenerator event_generator(GetRootWindow(view));
  event_generator.set_flags(flags);
  event_generator.MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

base::FilePath CreateFile(Profile* profile, const std::string& extension) {
  const base::FilePath file_path =
      GetDownloadsPath(profile).Append(base::StrCat(
          {base::UnguessableToken::Create().ToString(), ".", extension}));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::CreateDirectory(file_path.DirName())) {
      ADD_FAILURE() << "Failed to create parent directory.";
      return base::FilePath();
    }
    if (!base::WriteFile(file_path, /*data=*/std::string())) {
      ADD_FAILURE() << "Filed to write file contents.";
      return base::FilePath();
    }
  }

  return file_path;
}

void MoveMouseTo(const views::View* view, size_t count) {
  ui::test::EventGenerator(GetRootWindow(view))
      .MoveMouseTo(view->GetBoundsInScreen().CenterPoint(), count);
}

}  // namespace ash::test
