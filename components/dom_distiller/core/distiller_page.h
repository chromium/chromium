// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_PAGE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_PAGE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace dom_distiller {

class SourcePageHandle {
 public:
  virtual ~SourcePageHandle() = default;
};

// Injects JavaScript into a page, and uses it to extract and return long-form
// content. The class can be reused to load and distill multiple pages,
// following the state transitions described along with the class's states.
// Constructing a DistillerPage should be cheap, as some of the instances can be
// thrown away without ever being used.
class DistillerPage {
 public:
  using DistillerPageCallback = base::OnceCallback<void(
      std::unique_ptr<proto::DomDistillerResult> distilled_page,
      bool distillation_successful)>;

  DistillerPage();
  virtual ~DistillerPage();

  // Loads a URL. |OnDistillationDone| is called when the load completes or
  // fails. May be called when the distiller is idle. Callers can assume that,
  // for a given |url| and |options|, any DistillerPage implementation will
  // extract the same content.
  void DistillPage(const GURL& url,
                   const proto::DomDistillerOptions options,
                   DistillerPageCallback callback);

  // Called when the JavaScript execution completes. |page_url| is the url of
  // the distilled page. |value| contains data returned by the script.
  virtual void OnDistillationDone(const GURL& page_url,
                                  const base::Value* value);

  DistillerPage(const DistillerPage&) = delete;
  DistillerPage& operator=(const DistillerPage&) = delete;

 protected:
  // Called by |DistillPage| to carry out platform-specific instructions to load
  // and distill the |url| using the provided |script|. The extracted content
  // should be the same regardless of the DistillerPage implementation.
  virtual void DistillPageImpl(const GURL& url, const std::string& script) = 0;

  // The value returned between the JavaScript and the DistillerPage can be
  // either a dictionary with all the content, or a stringified version.
  virtual bool StringifyOutput() = 0;

 private:
  bool ready_;
  DistillerPageCallback distiller_page_callback_;
  base::TimeTicks distillation_start_;
};

// Factory for generating a |DistillerPage|.
class DistillerPageFactory {
 public:
  virtual ~DistillerPageFactory();

  // Constructs and returns a new DistillerPage. The implementation of this
  // should be very cheap, since the pages can be thrown away without being
  // used.
  virtual std::unique_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& render_view_size) const = 0;
  virtual std::unique_ptr<DistillerPage> CreateDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) const = 0;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLER_PAGE_H_
