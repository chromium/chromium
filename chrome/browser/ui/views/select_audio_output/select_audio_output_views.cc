// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_audio_output/select_audio_output_views.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/views/media_picker_utils.h"
#include "chrome/browser/ui/views/select_audio_output/select_audio_output_dialog.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

void SelectAudioOutputPickerViews::Show(
    Browser* browser,
    const content::SelectAudioOutputRequest& request,
    content::SelectAudioOutputCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* const rfh =
      content::RenderFrameHost::FromID(request.render_frame_host_id());
  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  CreateMediaPickerDialogWidget(
      browser, web_contents,
      new SelectAudioOutputDialog(request.audio_output_devices(),
                                  std::move(callback)),
      web_contents->GetTopLevelNativeWindow(),
      web_contents->GetContentNativeView());
}
