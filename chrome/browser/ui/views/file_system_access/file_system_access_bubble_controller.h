// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_BUBBLE_CONTROLLER_H_

class Browser;

/** FileSystemAccessBubbleController class hosting helper methods to assist with
 * the page action such as the logic for showing the action bubble. */

// TODO(crbug.com/376282751) Implement the rest of the page action controller
// class
class FileSystemAccessBubbleController {
 public:
  // Triggers the file system access bubble to be shown for the provided
  // browser's active web contents.
  static void Show(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_BUBBLE_CONTROLLER_H_
