// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printer_capabilities_mac.h"

#include "base/apple/foundation_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace printing {

namespace {

base::FilePath WriteOutCustomPapersPlist(const base::FilePath& dir,
                                         const char* name,
                                         NSDictionary* dict) {
  base::FilePath path = dir.Append(name);
  if (![dict writeToURL:base::apple::FilePathToNSURL(path) error:nil]) {
    path.clear();
  }
  return path;
}

}  // namespace

TEST(PrinterCapabilitiesMacTest, GetMacCustomPaperSizesFromFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @144,
        @"height" : @288,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "good1.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(1u, papers.size());
    EXPECT_EQ("foo", papers[0].display_name());
    EXPECT_EQ("", papers[0].vendor_id());
    EXPECT_EQ(50800, papers[0].size_um().width());
    EXPECT_EQ(101600, papers[0].size_um().height());
    EXPECT_EQ(gfx::Rect(0, 0, 50800, 101600), papers[0].printable_area_um());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @100,
        @"height" : @200,
        @"name" : @"bar",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "good2.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(1u, papers.size());
    EXPECT_EQ("bar", papers[0].display_name());
    EXPECT_EQ("", papers[0].vendor_id());
    EXPECT_EQ(35278, papers[0].size_um().width());
    EXPECT_EQ(70556, papers[0].size_um().height());
    EXPECT_EQ(gfx::Rect(0, 0, 35278, 70556), papers[0].printable_area_um());
  }
  {
    NSDictionary* dict = @{};
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "empty.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"height" : @200,
        @"name" : @"bar",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "no_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @100,
        @"name" : @"bar",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "no_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @100,
        @"height" : @200,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "no_name.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @0,
        @"height" : @200,
        @"name" : @"bar",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "zero_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @100,
        @"height" : @0,
        @"name" : @"bar",
      }
    };
    base::FilePath path = WriteOutCustomPapersPlist(temp_dir.GetPath(),
                                                    "zero_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @7199929,
        @"height" : @200,
        @"name" : @"bar",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @100,
        @"height" : @7199929,
        @"name" : @"bar",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"width" : @100,
        @"height" : @200,
        @"name" : @"",
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "empty_name.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
}

TEST(PrinterCapabilitiesMacTest, SortMacCustomPaperSizes) {
  base::FilePath unsorted_plist;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &unsorted_plist);
  unsorted_plist = unsorted_plist.AppendASCII("components")
                       .AppendASCII("test")
                       .AppendASCII("data")
                       .AppendASCII("printing")
                       .AppendASCII("unsorted_custompapers.plist");

  auto papers = internal::GetMacCustomPaperSizesFromFile(unsorted_plist);
  ASSERT_EQ(6u, papers.size());
  EXPECT_EQ("123", papers[0].display_name());
  EXPECT_EQ("Another Size", papers[1].display_name());
  EXPECT_EQ("Custom 11x11", papers[2].display_name());
  EXPECT_EQ("Size 3", papers[3].display_name());
  EXPECT_EQ("size 3", papers[4].display_name());
  EXPECT_EQ("\xC3\xA1nother size", papers[5].display_name());
}

TEST(PrinterCapabilitiesMacTest, GetMacCustomPaperSizesWithSetMargins) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @144,
        @"height" : @288,
        @"left" : @12,
        @"bottom" : @36,
        @"right" : @24,
        @"top" : @48,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "good1.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(1u, papers.size());
    EXPECT_EQ("foo", papers[0].display_name());
    EXPECT_EQ("", papers[0].vendor_id());
    EXPECT_EQ(50800, papers[0].size_um().width());
    EXPECT_EQ(101600, papers[0].size_um().height());
    EXPECT_EQ(gfx::Rect(4233, 12700, 38100, 71967),
              papers[0].printable_area_um());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @72,
        @"bottom" : @72,
        @"right" : @72,
        @"top" : @72,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "good2.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(1u, papers.size());
    EXPECT_EQ("foo", papers[0].display_name());
    EXPECT_EQ("", papers[0].vendor_id());
    EXPECT_EQ(215900, papers[0].size_um().width());
    EXPECT_EQ(279400, papers[0].size_um().height());
    EXPECT_EQ(gfx::Rect(25400, 25400, 165100, 228600),
              papers[0].printable_area_um());
  }
}

TEST(PrinterCapabilitiesMacTest, GetMacCustomPaperSizesMissingMargins) {
  // Any missing margins should be set to 0.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  NSDictionary* dict = @{
    @"foo" : @{
      @"name" : @"foo",
      @"width" : @612,
      @"height" : @792,
    }
  };
  base::FilePath path =
      WriteOutCustomPapersPlist(temp_dir.GetPath(), "missing.plist", dict);
  ASSERT_FALSE(path.empty());
  auto papers = internal::GetMacCustomPaperSizesFromFile(path);
  ASSERT_EQ(1u, papers.size());
  EXPECT_EQ("foo", papers[0].display_name());
  EXPECT_EQ("", papers[0].vendor_id());
  EXPECT_EQ(215900, papers[0].size_um().width());
  EXPECT_EQ(279400, papers[0].size_um().height());
  EXPECT_EQ(gfx::Rect(0, 0, 215900, 279400), papers[0].printable_area_um());
}

TEST(PrinterCapabilitiesMacTest, GetMacCustomPaperSizesOutOfBoundsMargins) {
  // Papers with out-of-bounds margins should be skipped.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @612,
        @"bottom" : @0,
        @"right" : @0,
        @"top" : @0,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_left.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @0,
        @"bottom" : @792,
        @"right" : @0,
        @"top" : @0,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_bottom.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @0,
        @"bottom" : @0,
        @"right" : @612,
        @"top" : @0,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_right.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @0,
        @"bottom" : @0,
        @"right" : @0,
        @"top" : @792,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_top.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @600,
        @"bottom" : @0,
        @"right" : @12,
        @"top" : @0,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_width.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
  {
    NSDictionary* dict = @{
      @"foo" : @{
        @"name" : @"foo",
        @"width" : @612,
        @"height" : @792,
        @"left" : @0,
        @"bottom" : @700,
        @"right" : @0,
        @"top" : @92,
      }
    };
    base::FilePath path =
        WriteOutCustomPapersPlist(temp_dir.GetPath(), "big_height.plist", dict);
    ASSERT_FALSE(path.empty());
    auto papers = internal::GetMacCustomPaperSizesFromFile(path);
    ASSERT_EQ(0u, papers.size());
  }
}

TEST(PrinterCapabilitiesMacTest, GetMacCustomPaperSizesEmptyMargins) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  NSDictionary* dict = @{
    @"foo" : @{
      @"name" : @"foo",
      @"width" : @144,
      @"height" : @288,
      @"left" : @0,
      @"bottom" : @0,
      @"right" : @0,
      @"top" : @0,
    }
  };
  base::FilePath path =
      WriteOutCustomPapersPlist(temp_dir.GetPath(), "empty.plist", dict);
  ASSERT_FALSE(path.empty());
  auto papers = internal::GetMacCustomPaperSizesFromFile(path);
  ASSERT_EQ(1u, papers.size());
  EXPECT_EQ("foo", papers[0].display_name());
  EXPECT_EQ("", papers[0].vendor_id());
  EXPECT_EQ(50800, papers[0].size_um().width());
  EXPECT_EQ(101600, papers[0].size_um().height());
  EXPECT_EQ(gfx::Rect(0, 0, 50800, 101600), papers[0].printable_area_um());
}

}  // namespace printing
