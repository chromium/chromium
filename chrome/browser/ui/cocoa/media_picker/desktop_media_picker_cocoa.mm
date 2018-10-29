// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_cocoa.h"

#include <utility>

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_controller.h"

DesktopMediaPickerCocoa::DesktopMediaPickerCocoa() {
}

DesktopMediaPickerCocoa::~DesktopMediaPickerCocoa() {
}

void DesktopMediaPickerCocoa::Show(
    const DesktopMediaPicker::Params& params,
    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
    const DoneCallback& done_callback) {
  controller_.reset([[DesktopMediaPickerController alloc]
      initWithSourceLists:std::move(source_lists)
                   parent:params.parent.GetNativeNSWindow()
                 callback:done_callback
                  appName:params.app_name
               targetName:params.target_name
             requestAudio:params.request_audio]);
  [controller_ showWindow:nil];
}

// static
std::unique_ptr<DesktopMediaPicker> DesktopMediaPicker::Create() {
  return std::unique_ptr<DesktopMediaPicker>(new DesktopMediaPickerCocoa());
}
