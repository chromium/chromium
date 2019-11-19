// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/printer_capabilities_mac.h"

#import <AppKit/AppKit.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "printing/units.h"

namespace printing {

namespace {

// On macOS, the custom paper size UI limits the value to 99999.
constexpr int kMacPaperDimensionLimit = 99999 * kPointsPerInch;

PrinterSemanticCapsAndDefaults::Papers& GetTestPapers() {
  static base::NoDestructor<PrinterSemanticCapsAndDefaults::Papers> test_papers;
  return *test_papers;
}

}  // namespace

PrinterSemanticCapsAndDefaults::Papers GetMacCustomPaperSizes() {
  if (!GetTestPapers().empty()) {
    return GetTestPapers();
  }

  base::FilePath local_library;
  bool success =
      base::mac::GetUserDirectory(NSLibraryDirectory, &local_library);
  DCHECK(success);

  base::FilePath plist = local_library.Append("Preferences")
                             .Append("com.apple.print.custompapers.plist");
  return internal::GetMacCustomPaperSizesFromFile(plist);
}

void SetMacCustomPaperSizesForTesting(
    const PrinterSemanticCapsAndDefaults::Papers& papers) {
  for (const PrinterSemanticCapsAndDefaults::Paper& paper : papers)
    DCHECK_EQ("", paper.vendor_id);

  GetTestPapers() = papers;
}

namespace internal {

PrinterSemanticCapsAndDefaults::Papers GetMacCustomPaperSizesFromFile(
    const base::FilePath& path) {
  PrinterSemanticCapsAndDefaults::Papers custom_paper_sizes;

  base::scoped_nsobject<NSDictionary> custom_papers_dict;
  {
    base::ScopedBlockingCall scoped_block(FROM_HERE,
                                          base::BlockingType::MAY_BLOCK);
    custom_papers_dict.reset([[NSDictionary alloc]
        initWithContentsOfFile:base::mac::FilePathToNSString(path)]);
  }

  for (id key in custom_papers_dict.get()) {
    NSDictionary* paper = [custom_papers_dict objectForKey:key];
    if (![paper isKindOfClass:[NSDictionary class]])
      continue;

    int width = [[paper objectForKey:@"width"] intValue];
    int height = [[paper objectForKey:@"height"] intValue];
    if (width <= 0 || height <= 0 || width > kMacPaperDimensionLimit ||
        height > kMacPaperDimensionLimit) {
      continue;
    }

    NSString* name = [paper objectForKey:@"name"];
    if (![name isKindOfClass:[NSString class]] || [name length] == 0)
      continue;

    gfx::Size size_microns(
        ConvertUnit(width, kPointsPerInch, kMicronsPerInch),
        ConvertUnit(height, kPointsPerInch, kMicronsPerInch));
    custom_paper_sizes.push_back(
        {base::SysNSStringToUTF8(name), "", size_microns});
  }

  return custom_paper_sizes;
}

}  // namespace internal

}  // namespace printing
