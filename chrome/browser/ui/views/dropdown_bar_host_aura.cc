// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dropdown_bar_host.h"

#include "ui/aura/window.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

void DropdownBarHost::SetHostViewNative(views::View* host_view) {
  host_->GetNativeView()->SetProperty(views::kHostViewKey, host_view);
}
