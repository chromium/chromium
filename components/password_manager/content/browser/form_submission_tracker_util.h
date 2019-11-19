// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_SUBMISSION_TRACKER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_SUBMISSION_TRACKER_UTIL_H_

#include "ui/base/page_transition_types.h"

namespace password_manager {

class FormSubmissionObserver;

// Calls |observer| with information whether the navigation might be due to a
// form submision.
void NotifyDidNavigateMainFrame(bool is_renderer_initiated,
                                ui::PageTransition transition,
                                bool was_initiated_by_link_click,
                                FormSubmissionObserver* observer);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_FORM_SUBMISSION_TRACKER_UTIL_H_
