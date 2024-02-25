// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_SELECT_FILE_DIALOG_BRIDGE_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_SELECT_FILE_DIALOG_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/select_file_dialog.mojom.h"

@class ExtensionDropdownHandler;
@class SelectFileDialogDelegate;

namespace remote_cocoa {

// This structure bridges from the C++ mojo SelectFileDialog to the Cocoa
// NSSavePanel.
class REMOTE_COCOA_APP_SHIM_EXPORT SelectFileDialogBridge
    : public mojom::SelectFileDialog {
 public:
  explicit SelectFileDialogBridge(NSWindow* owning_window);

  SelectFileDialogBridge(const SelectFileDialogBridge&) = delete;
  SelectFileDialogBridge& operator=(const SelectFileDialogBridge&) = delete;

  ~SelectFileDialogBridge() override;

  // mojom::SelectFileDialog:
  void Show(mojom::SelectFileDialogType type,
            const std::u16string& title,
            const base::FilePath& default_path,
            mojom::SelectFileTypeInfoPtr file_types,
            int file_type_index,
            const base::FilePath::StringType& default_extension,
            ShowCallback callback) override;

  // Return the most recently created NSSavePanel. There is no guarantee that
  // this will be a valid pointer, and is only to be used to bridge across the
  // mojo interface for testing.
  static NSSavePanel* GetLastCreatedNativePanelForTesting();

 private:
  // Sets the accessory view for |dialog_| and sets
  // |extension_dropdown_handler_|. |is_save_panel| specifies whether this is
  // for a save panel or not.
  void SetAccessoryView(mojom::SelectFileTypeInfoPtr file_types,
                        int file_type_index,
                        const base::FilePath::StringType& default_extension,
                        bool is_save_panel);

  // Called when the panel completes.
  void OnPanelEnded(bool did_cancel);

  // The callback to make when this dialog ends.
  ShowCallback show_callback_;

  // Type type of this dialog.
  mojom::SelectFileDialogType type_;

  // The NSSavePanel that |this| tracks.
  NSSavePanel* __strong panel_;

  // The parent window for |panel_|.
  NSWindow* __strong owning_window_;

  // The delegate for |panel|.
  SelectFileDialogDelegate* __strong delegate_;

  // Extension dropdown handler corresponding to this file dialog.
  ExtensionDropdownHandler* __strong extension_dropdown_handler_;

  base::WeakPtrFactory<SelectFileDialogBridge> weak_factory_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_SELECT_FILE_DIALOG_BRIDGE_H_
