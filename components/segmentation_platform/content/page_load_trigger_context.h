// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_CONTENT_PAGE_LOAD_TRIGGER_CONTEXT_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_CONTENT_PAGE_LOAD_TRIGGER_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/public/trigger_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}  // namespace content

namespace segmentation_platform {

// Contains contextual information for a page load trigger event.
struct PageLoadTriggerContext : public TriggerContext {
 public:
  explicit PageLoadTriggerContext(content::WebContents* web_contents);
  ~PageLoadTriggerContext() override;

  base::flat_map<std::string, processing::ProcessedValue>
  GetSelectionInputArgs() const override;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject() const override;
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_CONTENT_PAGE_LOAD_TRIGGER_CONTEXT_H_
