// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printer_capabilities_mac.h"

#import <AppKit/AppKit.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

// On macOS, the custom paper size UI limits the value to 99999.
constexpr int kMacPaperDimensionLimit = 99999 * kPointsPerInch;

PrinterSemanticCapsAndDefaults::Papers& GetTestPapers() {
  static base::NoDestructor<PrinterSemanticCapsAndDefaults::Papers> test_papers;
  return *test_papers;
}

bool IsValidMargin(int margin) {
  return 0 <= margin && margin <= kMacPaperDimensionLimit;
}

}  // namespace

PrinterSemanticCapsAndDefaults::Papers GetMacCustomPaperSizes() {
  if (!GetTestPapers().empty()) {
    return GetTestPapers();
  }

  base::FilePath local_library;
  bool success =
      base::apple::GetUserDirectory(NSLibraryDirectory, &local_library);
  DCHECK(success);

  base::FilePath plist = local_library.Append("Preferences")
                             .Append("com.apple.print.custompapers.plist");
  return internal::GetMacCustomPaperSizesFromFile(plist);
}

void SetMacCustomPaperSizesForTesting(
    const PrinterSemanticCapsAndDefaults::Papers& papers) {
  for (const PrinterSemanticCapsAndDefaults::Paper& paper : papers)
    DCHECK_EQ("", paper.vendor_id());

  GetTestPapers() = papers;
}

namespace internal {

PrinterSemanticCapsAndDefaults::Papers GetMacCustomPaperSizesFromFile(
    const base::FilePath& path) {
  PrinterSemanticCapsAndDefaults::Papers custom_paper_sizes;

  NSDictionary* custom_papers_dict;
  {
    base::ScopedBlockingCall scoped_block(FROM_HERE,
                                          base::BlockingType::MAY_BLOCK);
    custom_papers_dict = [[NSDictionary alloc]
        initWithContentsOfURL:base::apple::FilePathToNSURL(path)
                        error:nil];
    if (!custom_papers_dict) {
      return custom_paper_sizes;
    }
  }

  for (id key in custom_papers_dict) {
    NSDictionary* paper = base::apple::ObjCCast<NSDictionary>(
        [custom_papers_dict objectForKey:key]);
    if (!paper) {
      continue;
    }

    int size_width = [paper[@"width"] intValue];
    int size_height = [paper[@"height"] intValue];
    if (size_width <= 0 || size_height <= 0 ||
        size_width > kMacPaperDimensionLimit ||
        size_height > kMacPaperDimensionLimit) {
      continue;
    }

    NSString* name = paper[@"name"];
    if (![name isKindOfClass:[NSString class]] || name.length == 0) {
      continue;
    }

    gfx::Size size_microns(
        ConvertUnit(size_width, kPointsPerInch, kMicronsPerInch),
        ConvertUnit(size_height, kPointsPerInch, kMicronsPerInch));

    int margin_left = [paper[@"left"] intValue];
    int margin_bottom = [paper[@"bottom"] intValue];
    int margin_right = [paper[@"right"] intValue];
    int margin_top = [paper[@"top"] intValue];
    if (!IsValidMargin(margin_left) || !IsValidMargin(margin_bottom) ||
        !IsValidMargin(margin_right) || !IsValidMargin(margin_top)) {
      continue;
    }

    // Since each margin must be less than `kMacPaperDimensionLimit`, there
    // won't be any integer overflow here.
    int margin_width = margin_left + margin_right;
    int margin_height = margin_bottom + margin_top;
    if (margin_width >= size_width || margin_height >= size_height) {
      continue;
    }

    // The printable area should now always be non-empty and always in-bounds of
    // the paper size.
    int printable_area_width = size_width - margin_width;
    int printable_area_height = size_height - margin_height;
    gfx::Rect printable_area_microns(
        ConvertUnit(margin_left, kPointsPerInch, kMicronsPerInch),
        ConvertUnit(margin_bottom, kPointsPerInch, kMicronsPerInch),
        ConvertUnit(printable_area_width, kPointsPerInch, kMicronsPerInch),
        ConvertUnit(printable_area_height, kPointsPerInch, kMicronsPerInch));

    custom_paper_sizes.emplace_back(base::SysNSStringToUTF8(name),
                                    /*vendor_id=*/"", size_microns,
                                    printable_area_microns);
  }
  std::sort(custom_paper_sizes.begin(), custom_paper_sizes.end(),
            [](const PrinterSemanticCapsAndDefaults::Paper& a,
               const PrinterSemanticCapsAndDefaults::Paper& b) {
              return a.display_name() < b.display_name();
            });

  return custom_paper_sizes;
}

}  // namespace internal

}  // namespace printing
