// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1008939): These traits files, printing_param_traits.{cc,h},
// are required for sending printing mojom types to legacy IPCs. Once the
// mojofication of printing is done, these traits should be removed.

#ifndef COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_H_
#define COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_H_

#include "components/printing/common/print.mojom.h"
#include "ipc/ipc_message_utils.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace IPC {

template <>
struct ParamTraits<printing::mojom::DidPrintContentParamsPtr> {
  typedef printing::mojom::DidPrintContentParamsPtr param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<printing::mojom::PrintParamsPtr> {
  typedef printing::mojom::PrintParamsPtr param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // COMPONENTS_PRINTING_COMMON_PRINTING_PARAM_TRAITS_H_
