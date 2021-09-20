// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_ERROR_HANDLER_IMPL_H__
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_ERROR_HANDLER_IMPL_H__

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/model/data_type_error_handler.h"

namespace syncer {

// The standard implementation of DataTypeErrorHandler.
class DataTypeErrorHandlerImpl : public DataTypeErrorHandler {
 public:
  using ErrorCallback = base::RepeatingCallback<void(const SyncError&)>;

  DataTypeErrorHandlerImpl(
      const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
      const base::RepeatingClosure& dump_stack,
      const ErrorCallback& sync_callback);
  ~DataTypeErrorHandlerImpl() override;

  void OnUnrecoverableError(const SyncError& error) override;
  SyncError CreateAndUploadError(const base::Location& location,
                                 const std::string& message,
                                 ModelType type) override;
  std::unique_ptr<DataTypeErrorHandler> Copy() const override;

 private:
  // The thread task runner that |sync_callback_| runs on. This is passed in
  // separately instead of bound inside the callback because we want to be able
  // to perform the PostTask using the error location.
  scoped_refptr<base::SequencedTaskRunner> ui_thread_;

  // The callback to dump and upload the stack from the current thread.
  base::RepeatingClosure dump_stack_;

  // The callback used to inform sync of the error on the |ui_thread_|.
  ErrorCallback sync_callback_;

  DISALLOW_COPY_AND_ASSIGN(DataTypeErrorHandlerImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_ERROR_HANDLER_IMPL_H__
