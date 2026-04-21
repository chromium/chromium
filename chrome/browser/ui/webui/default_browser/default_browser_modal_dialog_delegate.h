// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class Widget;
}

namespace default_browser {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kDefaultBrowserModalDialogId);

// The "Modal" entrypoint for the Windows Default Browser. This dialog
// displays a WebUI page to provide prompts that help users navigate OS
// settings to set Chrome as the default browser based on the interaction
// choice.
// Shows the modal dialog. This is triggered by the DefaultBrowserManager when
// the STARTUP_MODAL entrypoint is selected.
std::unique_ptr<views::Widget> Show(Profile* profile,
                                    gfx::NativeWindow parent,
                                    bool use_settings_illustration,
                                    bool can_pin_to_taskbar);

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_
