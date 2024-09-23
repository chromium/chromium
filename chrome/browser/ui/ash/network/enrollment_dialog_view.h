// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_ENROLLMENT_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_ENROLLMENT_DIALOG_VIEW_H_

#include <string>

namespace ash::enrollment {

inline constexpr char kWidgetName[] = "EnrollmentDialogWidget";

// Creates and shows the dialog for certificate-based network enrollment.
bool CreateEnrollmentDialog(const std::string& network_id);

}  // namespace ash::enrollment

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_ENROLLMENT_DIALOG_VIEW_H_
