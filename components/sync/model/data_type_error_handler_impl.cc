// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/data_type_error_handler_impl.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"

namespace syncer {

DataTypeErrorHandlerImpl::DataTypeErrorHandlerImpl(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const base::Closure& dump_stack,
    const ErrorCallback& sync_callback)
    : ui_thread_(ui_thread),
      dump_stack_(dump_stack),
      sync_callback_(sync_callback) {}

DataTypeErrorHandlerImpl::~DataTypeErrorHandlerImpl() {}

void DataTypeErrorHandlerImpl::OnUnrecoverableError(const SyncError& error) {
  if (!dump_stack_.is_null())
    dump_stack_.Run();
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures2",
                            ModelTypeHistogramValue(error.model_type()));
  ui_thread_->PostTask(error.location(), base::BindOnce(sync_callback_, error));
}

SyncError DataTypeErrorHandlerImpl::CreateAndUploadError(
    const base::Location& location,
    const std::string& message,
    ModelType type) {
  if (!dump_stack_.is_null())
    dump_stack_.Run();
  return SyncError(location, SyncError::DATATYPE_ERROR, message, type);
}

std::unique_ptr<DataTypeErrorHandler> DataTypeErrorHandlerImpl::Copy() const {
  return std::make_unique<DataTypeErrorHandlerImpl>(ui_thread_, dump_stack_,
                                                    sync_callback_);
}

}  // namespace syncer
