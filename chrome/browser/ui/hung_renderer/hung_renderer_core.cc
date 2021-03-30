// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_renderer/hung_renderer_core.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

// Author's Note:
//
// The inefficiency of walking the frame tree repeatedly has not gone unnoticed.
// Given the different requirements of the Cocoa and Views implementations of
// the Hung Page dialog, this is the simplest way. When we consolidate
// implementations, it will be a good idea to reconsider this approach.

// Returns whether the WebContents has a frame that is backed by the hung
// process.
bool IsWebContentsHung(content::WebContents* web_contents,
                       content::RenderProcessHost* hung_process) {
  for (auto* frame : web_contents->GetAllFrames()) {
    if (frame->GetProcess() == hung_process)
      return true;
  }
  return false;
}

// Returns the URL of the first hung frame encountered in the WebContents.
GURL GetURLOfAnyHungFrame(content::WebContents* web_contents,
                          content::RenderProcessHost* hung_process) {
  for (auto* frame : web_contents->GetAllFrames()) {
    if (frame->GetProcess() == hung_process)
      return frame->GetLastCommittedURL();
  }

  // If a frame is attempting to commit a navigation into a hung renderer
  // process, then its |frame->GetProcess()| will still return the process
  // hosting the previously committed navigation.  In such case, the loop above
  // might not find any matching frame.
  return GURL();
}

}  // namespace

std::vector<content::WebContents*> GetHungWebContentsList(
    content::WebContents* hung_web_contents,
    content::RenderProcessHost* hung_process) {
  std::vector<content::WebContents*> result;

  auto is_hung = [hung_process](content::WebContents* web_contents) {
    return IsWebContentsHung(web_contents, hung_process) &&
           !web_contents->IsCrashed();
  };
  std::copy_if(AllTabContentses().begin(), AllTabContentses().end(),
               std::back_inserter(result), is_hung);

  // Move |hung_web_contents| to the front.  It might be missing from the
  // initial |results| when it hasn't yet committed a navigation into the hung
  // process.
  auto first = std::find(result.begin(), result.end(), hung_web_contents);
  if (first != result.end())
    std::rotate(result.begin(), first, std::next(first));
  else
    result.insert(result.begin(), hung_web_contents);

  return result;
}

// Given a WebContents that is affected by the hang, and the RenderProcessHost
// of the hung process, returns the title of the WebContents that should be used
// in the "Hung Page" dialog.
std::u16string GetHungWebContentsTitle(
    content::WebContents* affected_web_contents,
    content::RenderProcessHost* hung_process) {
  std::u16string page_title = affected_web_contents->GetTitle();
  if (page_title.empty())
    page_title = CoreTabHelper::GetDefaultTitle();
  // TODO(xji): Consider adding a special case if the title text is a URL,
  // since those should always have LTR directionality. Please refer to
  // http://crbug.com/6726 for more information.
  base::i18n::AdjustStringForLocaleDirection(&page_title);

  if (affected_web_contents->GetMainFrame()->GetProcess() == hung_process)
    return page_title;

  GURL hung_url = GetURLOfAnyHungFrame(affected_web_contents, hung_process);
  if (!hung_url.is_valid() || !hung_url.has_host())
    return page_title;

  // N.B. using just the host here is OK since this is a notification and the
  // user doesn't need to make a security critical decision about the page in
  // this dialog.
  std::u16string host_string;
  url_formatter::AppendFormattedHost(hung_url, &host_string);

  return l10n_util::GetStringFUTF16(IDS_BROWSER_HANGMONITOR_IFRAME_TITLE,
                                    host_string, page_title);
}
