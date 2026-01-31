// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_COMMON_PRINT_DIALOG_LINUX_PORTAL_H_
#define COMPONENTS_PRINTING_COMMON_PRINT_DIALOG_LINUX_PORTAL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/dbus/xdg/request.h"
#include "dbus/bus.h"
#include "printing/print_dialog_linux_interface.h"
#include "printing/printing_context_linux.h"
#include "ui/gfx/native_ui_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace printing {

// Implementation of PrintDialogLinuxInterface that uses the XDG Print Portal.
class COMPONENT_EXPORT(PRINTING) PrintDialogLinuxPortal
    : public PrintDialogLinuxInterface {
 public:
  explicit PrintDialogLinuxPortal(PrintingContextLinux* context,
                                  scoped_refptr<dbus::Bus> bus = nullptr);

  PrintDialogLinuxPortal(const PrintDialogLinuxPortal&) = delete;
  PrintDialogLinuxPortal& operator=(const PrintDialogLinuxPortal&) = delete;
  ~PrintDialogLinuxPortal() override;

  // PrintDialogLinuxInterface:
  void UseDefaultSettings() override;
  void UpdateSettings(std::unique_ptr<PrintSettings> settings) override;
#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  void LoadPrintSettings(const PrintSettings& settings) override;
#endif
  void ShowDialog(
      gfx::NativeView parent_view,
      bool has_selection,
      PrintingContextLinux::PrintSettingsCallback callback) override;
  void PrintDocument(const MetafilePlayer& metafile,
                     const std::u16string& document_name) override;

 private:
  // Checks if the portal is available and routes to the appropriate handler.
  void OnPortalAvailable(bool has_selection, uint32_t version);

  // Called when the window handle is exported.
  void OnWindowHandleExported(bool has_selection, std::string handle);

  // Response handlers for portal requests.
  void OnPreparePrintResponse(dbus_xdg::Results results);

  // Instantiates and delegates to the fallback dialog.
  void UseFallback(bool has_selection);

  raw_ptr<PrintingContextLinux> context_;
  PrintingContextLinux::PrintSettingsCallback callback_;

  std::unique_ptr<PrintSettings> settings_;

  // State for the portal request.
  std::unique_ptr<dbus_xdg::Request> request_;
  std::string parent_handle_;
  std::optional<uint32_t> token_;

  // The fallback dialog to use if the portal is not available.
  std::unique_ptr<PrintDialogLinuxInterface> fallback_dialog_;

  // Temporary storage for ShowDialog arguments while checking portal
  // availability.
  gfx::NativeView parent_view_ = nullptr;

  scoped_refptr<dbus::Bus> bus_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrintDialogLinuxPortal> weak_factory_{this};
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_COMMON_PRINT_DIALOG_LINUX_PORTAL_H_
