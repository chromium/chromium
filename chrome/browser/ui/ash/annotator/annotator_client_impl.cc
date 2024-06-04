// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/annotator/annotator_client_impl.h"

#include "ash/webui/annotator/untrusted_annotator_page_handler_impl.h"

AnnotatorClientImpl::AnnotatorClientImpl() = default;

AnnotatorClientImpl::~AnnotatorClientImpl() = default;

void AnnotatorClientImpl::SetAnnotatorPageHandler(
    ash::UntrustedAnnotatorPageHandlerImpl* handler) {
  annotator_handler_ = handler;
}

void AnnotatorClientImpl::ResetAnnotatorPageHandler(
    ash::UntrustedAnnotatorPageHandlerImpl* handler) {
  if (annotator_handler_ == handler) {
    annotator_handler_ = nullptr;
  }
}

void AnnotatorClientImpl::SetTool(const ash::AnnotatorTool& tool) {
  DCHECK(annotator_handler_);
  annotator_handler_->SetTool(tool);
}

void AnnotatorClientImpl::Clear() {
  DCHECK(annotator_handler_);
  annotator_handler_->Clear();
}
