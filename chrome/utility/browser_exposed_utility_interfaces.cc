// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/browser_exposed_utility_interfaces.h"

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(IS_WIN)
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/services/printing/pdf_to_emf_converter_factory.h"
#endif

void ExposeElevatedChromeUtilityInterfacesToBrowser(mojo::BinderMap* binders) {
#if BUILDFLAG(ENABLE_PRINTING) && BUILDFLAG(IS_WIN)
  binders->Add<printing::mojom::PdfToEmfConverterFactory>(
      base::BindRepeating(printing::PdfToEmfConverterFactory::Create),
      base::ThreadTaskRunnerHandle::Get());
#endif
}
