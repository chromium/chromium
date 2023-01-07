// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_EVENT_SINK_IMPL_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_EVENT_SINK_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/process/process.h"
#include "chrome/common/conflicts/module_event_sink_win.mojom.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class FilePath;
}

// Implementation of the mojom::ModuleEventSink interface. This is the endpoint
// in the browser process. This redirects module events to the provided
// callback.
class ModuleEventSinkImpl : public mojom::ModuleEventSink {
 public:
  // Callback for retrieving the handle associated with a process. This is used
  // by "Create" to get a handle to the remote process.
  using GetProcessCallback = base::RepeatingCallback<base::Process()>;

  using OnModuleLoadCallback =
      base::RepeatingCallback<void(content::ProcessType process_type,
                                   const base::FilePath& module_path,
                                   uint32_t module_size,
                                   uint32_t module_time_date_stamp)>;

  // Creates a service endpoint that forwards notifications from the remote
  // |process| of the provided |process_type| to the provided
  // |on_module_load_callback|.
  ModuleEventSinkImpl(base::Process process,
                      content::ProcessType process_type,
                      const OnModuleLoadCallback& on_module_load_callback);

  ModuleEventSinkImpl(const ModuleEventSinkImpl&) = delete;
  ModuleEventSinkImpl& operator=(const ModuleEventSinkImpl&) = delete;

  ~ModuleEventSinkImpl() override;

  // Factory function for use with service_manager::InterfaceRegistry. This
  // creates a concrete implementation of mojom::ModuleEventSink interface in
  // the current process, for the remote process represented by the provided
  // |request|. This should only be called on the UI thread.
  static void Create(GetProcessCallback get_process,
                     content::ProcessType process_type,
                     const OnModuleLoadCallback& on_module_load_callback,
                     mojo::PendingReceiver<mojom::ModuleEventSink> receiver);

  // mojom::ModuleEventSink implementation:
  void OnModuleEvents(
      const std::vector<uint64_t>& module_load_addresses) override;

 private:
  friend class ModuleEventSinkImplTest;

  // A handle to the process on the other side of the pipe.
  base::Process process_;

  // The process ID of the remote process on the other end of the pipe. This is
  // forwarded along to the ModuleDatabase for each call.
  content::ProcessType process_type_;

  // The callback this forwards events to.
  OnModuleLoadCallback on_module_load_callback_;
};

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_EVENT_SINK_IMPL_H_
