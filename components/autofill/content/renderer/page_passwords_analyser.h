// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_PASSWORDS_ANALYSER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_PASSWORDS_ANALYSER_H_

#include <set>

#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace autofill {

class PageFormAnalyserLogger;

// This class provides feedback to web developers about the password forms on
// their webpages, in order to increase the accessibility of web forms to the
// Password Manager. This is achieved by crawling the DOM whenever new forms are
// added to the page and checking for common mistakes, or ways in which the form
// could be improved (for example, with autocomplete attributes). See
// |AnalyseDocumentDOM| for more specific information about these warnings.
class PagePasswordsAnalyser {
 public:
  PagePasswordsAnalyser();

  ~PagePasswordsAnalyser();

  // Clear the set of nodes that have already been analysed, so that they will
  // be analysed again next time |AnalyseDocumentDOM| is called. This is called
  // upon page load, for instance.
  void Reset();

  // |AnalyseDocumentDOM| traverses the DOM, logging potential issues in the
  // DevTools console. Errors are logged for those issues that conflict with the
  // HTML specification. Warnings are logged for issues that cause problems with
  // identification of fields on the web-page for the Password Manager.
  // Warning and error messages are logged to |logger|.
  void AnalyseDocumentDOM(blink::WebLocalFrame* frame,
                          PageFormAnalyserLogger* logger);

  // By default, the analyser will log to the DevTools console.
  void AnalyseDocumentDOM(blink::WebLocalFrame* frame);

  // A set of renderer_ids which have already been analyzed.
  std::set<uint32_t> skip_control_element_renderer_ids_;
  std::set<uint32_t> skip_form_element_renderer_ids_;
  // This is true when new DOM content is available since the last time
  // the page was analysed, meaning the page needs to be reanalysed.
  bool page_dirty_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_PASSWORDS_ANALYSER_H_
