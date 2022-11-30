// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_parser.h"

#include "base/json/json_reader.h"
#include "base/strings/string_piece.h"
#include "chromeos/printing/ppd_metadata_matchers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

constexpr base::StringPiece kInvalidJson = "blah blah invalid JSON";

// Verifies that ParseLocales() can parse locales metadata.
TEST(PpdMetadataParserTest, CanParseLocales) {
  constexpr base::StringPiece kLocalesJson = R"(
  {
    "locales": [ "de", "en", "es", "jp" ]
  }
  )";

  const auto parsed = ParseLocales(kLocalesJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              ElementsAre(StrEq("de"), StrEq("en"), StrEq("es"), StrEq("jp")));
}

// Verifies that ParseLocales() can parse locales and return a partial
// list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseLocales) {
  // The values "0.0" and "78" are gibberish that ParseLocales() shall
  // ignore; however, these don't structurally foul the JSON, so it can
  // still return the other locales.
  constexpr base::StringPiece kLocalesJson = R"(
  {
    "locales": [ 0.0, "de", 78, "en", "es", "jp" ]
  }
  )";

  const auto parsed = ParseLocales(kLocalesJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              ElementsAre(StrEq("de"), StrEq("en"), StrEq("es"), StrEq("jp")));
}

// Verifies that ParseLocales() returns absl::nullopt rather than an
// empty container.
TEST(PpdMetadataParserTest, ParseLocalesDoesNotReturnEmptyContainer) {
  // The values "0.0" and "78" are gibberish that ParseLocales() shall
  // ignore; while the JSON is still well-formed, the parsed list of
  // locales contains no values.
  constexpr base::StringPiece kLocalesJson = R"(
  {
    "locales": [ 0.0, 78 ]
  }
  )";

  EXPECT_FALSE(ParseLocales(kLocalesJson).has_value());
}

// Verifies that ParseLocales() returns absl::nullopt on irrecoverable
// parse error.
TEST(PpdMetadataParserTest, ParseLocalesFailsGracefully) {
  EXPECT_FALSE(ParseLocales(kInvalidJson).has_value());
}

// Verifies that ParseManufacturers() can parse manufacturers metadata.
TEST(PpdMetadataParserTest, CanParseManufacturers) {
  constexpr base::StringPiece kManufacturersJson = R"(
  {
    "filesMap": {
      "Andante": "andante-en.json",
      "Sostenuto": "sostenuto-en.json"
    }
  }
  )";

  const auto parsed = ParseManufacturers(kManufacturersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  Pair(StrEq("Andante"), StrEq("andante-en.json")),
                  Pair(StrEq("Sostenuto"), StrEq("sostenuto-en.json"))));
}

// Verifies that ParseManufacturers() can parse manufacturers and return
// a partial list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseManufacturers) {
  // Contains an embedded dictionary keyed on "Dearie me."
  // ParseManufacturers() shall ignore this.
  constexpr base::StringPiece kManufacturersJson = R"(
  {
    "filesMap": {
      "Dearie me": {
        "I didn't": "expect",
        "to go": "deeper"
      },
      "Andante": "andante-en.json",
      "Sostenuto": "sostenuto-en.json"
    }
  }
  )";

  const auto parsed = ParseManufacturers(kManufacturersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  Pair(StrEq("Andante"), StrEq("andante-en.json")),
                  Pair(StrEq("Sostenuto"), StrEq("sostenuto-en.json"))));
}

// Verifies that ParseManufacturers() returns absl::nullopt rather than
// an empty container.
TEST(PpdMetadataParserTest, ParseManufacturersDoesNotReturnEmptyContainer) {
  // Contains an embedded dictionary keyed on "Dearie me."
  // ParseManufacturers() shall ignore this, but in doing so shall leave
  // its ParsedManufacturers return value empty.
  constexpr base::StringPiece kManufacturersJson = R"(
  {
    "filesMap": {
      "Dearie me": {
        "I didn't": "expect",
        "to go": "deeper"
      }
    }
  }
  )";

  EXPECT_FALSE(ParseManufacturers(kManufacturersJson).has_value());
}

// Verifies that ParseManufacturers() returns absl::nullopt on
// irrecoverable parse error.
TEST(PpdMetadataParserTest, ParseManufacturersFailsGracefully) {
  EXPECT_FALSE(ParseManufacturers(kInvalidJson).has_value());
}

// Verifies that ParsePrinters() can parse printers metadata.
TEST(PpdMetadataParserTest, CanParsePrinters) {
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "printers": [ {
      "emm": "d 547b",
      "name": "An die Musik"
    }, {
      "emm": "d 553",
      "name": "Auf der Donau"
    } ]
  }
  )";

  const auto parsed = ParsePrinters(kPrintersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed, UnorderedElementsAre(
                           ParsedPrinterLike("An die Musik", "d 547b"),
                           ParsedPrinterLike("Auf der Donau", "d 553")));
}

// Verifies that ParsePrinters() can parse printers and return a partial
// list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParsePrinters) {
  // Contains an extra value keyed on "hello" in an otherwise valid leaf
  // value in Printers metadata. ParsePrinters() shall ignore this.
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "printers": [ {
      "emm": "d 552",
      "name": "Hänflings Liebeswerbung",
      "hello": "there!"
    }, {
      "emm": "d 553",
      "name": "Auf der Donau"
    } ]
  }
  )";

  const auto parsed = ParsePrinters(kPrintersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  ParsedPrinterLike("Hänflings Liebeswerbung", "d 552"),
                  ParsedPrinterLike("Auf der Donau", "d 553")));
}

// Verifies that ParsePrinters() can parse printers and their
// well-formed restrictions (if any are specified).
TEST(PpdMetadataParserTest, CanParsePrintersWithRestrictions) {
  // Specifies
  // *  a printer with a minimum milestone,
  // *  a printer with a maximum milestone, and
  // *  a printer with both minimum and maximum milestones.
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "printers": [ {
      "emm": "d 121",
      "name": "Schäfers Klagelied",
      "restriction": {
        "minMilestone": 121
      }
    }, {
      "emm": "d 216",
      "name": "Meeres Stille",
      "restriction": {
        "maxMilestone": 216
      }
    }, {
      "emm": "d 257",
      "name": "Heidenröslein",
      "restriction": {
        "minMilestone": 216,
        "maxMilestone": 257
      }
    } ]
  }
  )";

  const auto parsed = ParsePrinters(kPrintersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed,
              UnorderedElementsAre(
                  AllOf(ParsedPrinterLike("Schäfers Klagelied", "d 121"),
                        Field(&ParsedPrinter::restrictions,
                              RestrictionsWithMinMilestone(121))),
                  AllOf(ParsedPrinterLike("Meeres Stille", "d 216"),
                        Field(&ParsedPrinter::restrictions,
                              RestrictionsWithMaxMilestone(216))),
                  AllOf(ParsedPrinterLike("Heidenröslein", "d 257"),
                        Field(&ParsedPrinter::restrictions,
                              RestrictionsWithMinAndMaxMilestones(216, 257)))));
}

// Verifies that ParsePrinters() can parse printers and ignore
// malformed restrictions.
TEST(PpdMetadataParserTest, CanParsePrintersWithMalformedRestrictions) {
  // Specifies a printer with invalid restrictions.
  constexpr base::StringPiece kPrintersJson = R"(
  {
    "printers": [ {
      "emm": "d 368",
      "name": "Jägers Abendlied",
      "restriction": {
        "hello": "there!"
      }
    } ]
  }
  )";

  const auto parsed = ParsePrinters(kPrintersJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(
      *parsed,
      UnorderedElementsAre(
          AllOf(ParsedPrinterLike("Jägers Abendlied", "d 368"),
                Field(&ParsedPrinter::restrictions, UnboundedRestrictions()))));
}

// Verifies that ParsePrinters() returns absl::nullopt rather than an
// empty container.
TEST(PpdMetadataParserTest, ParsePrintersDoesNotReturnEmptyContainer) {
  // No printers are specified in this otherwise valid JSON dictionary.
  EXPECT_FALSE(ParsePrinters("{}").has_value());
}

// Verifies that ParsePrinters() returns absl::nullopt on irrecoverable
// parse error.
TEST(PpdMetadataParserTest, ParsePrintersFailsGracefully) {
  EXPECT_FALSE(ParsePrinters(kInvalidJson).has_value());
}

// Verifies that ParseForwardIndex() can parse forward index metadata.
TEST(PpdMetadataParserTest, CanParseForwardIndex) {
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "der wanderer": {
      "ppdMetadata": [ {
        "name": "d-489.ppd.gz"
      } ]
    },
    "morgenlied": {
      "ppdMetadata": [ {
        "name": "d-685.ppd.gz"
      } ]
    },
    "wandrers nachtlied": {
      "ppdMetadata": [ {
        "name": "d-224.ppd.gz"
      } ]
    }
  }
})";

  const auto parsed = ParseForwardIndex(kJsonForwardIndex);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_THAT(
      parsed.value(),
      UnorderedElementsAre(
          ParsedIndexEntryLike(
              "der wanderer",
              UnorderedElementsAre(
                  ParsedIndexLeafWithPpdBasename("d-489.ppd.gz"))),
          ParsedIndexEntryLike(
              "morgenlied", UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                                "d-685.ppd.gz"))),
          ParsedIndexEntryLike(
              "wandrers nachtlied",
              UnorderedElementsAre(
                  ParsedIndexLeafWithPpdBasename("d-224.ppd.gz")))));
}

// Verifies that ParseForwardIndex() can parse forward index metadata
// and return a partial list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseForwardIndex) {
  // Uses the same value as the CanParseForwardIndex test, but
  // with garbage values mixed in.
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "garbage": "unused value",
    "more garbage": [ "more", "unused", "values" ],
    "der wanderer": {
      "ppdMetadata": [ {
        "name": "d-489.ppd.gz",
        "more garbage still": "unused value"
      }, {
        "also garbage": "unused value"
      } ],
      "unending garbage": "unused value"
    },
    "morgenlied": {
      "ppdMetadata": [ {
        "name": "d-685.ppd.gz"
      } ]
    },
    "wandrers nachtlied": {
      "ppdMetadata": [ {
        "name": "d-224.ppd.gz"
      } ]
    }
  }
})";

  const auto parsed = ParseForwardIndex(kJsonForwardIndex);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_THAT(
      parsed.value(),
      UnorderedElementsAre(
          ParsedIndexEntryLike(
              "der wanderer",
              UnorderedElementsAre(
                  ParsedIndexLeafWithPpdBasename("d-489.ppd.gz"))),
          ParsedIndexEntryLike(
              "morgenlied", UnorderedElementsAre(ParsedIndexLeafWithPpdBasename(
                                "d-685.ppd.gz"))),
          ParsedIndexEntryLike(
              "wandrers nachtlied",
              UnorderedElementsAre(
                  ParsedIndexLeafWithPpdBasename("d-224.ppd.gz")))));
}

// Verifies that ParseForwardIndex() can parse forward index metadata
// in which leaf values have multiple ppdMetadata key-value pairs.
TEST(PpdMetadataParserTest, CanParseForwardIndexWithMultiplePpdMetadataLeafs) {
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "rastlose liebe": {
      "ppdMetadata": [ {
        "name": "d-138.ppd.gz"
      }, {
        "name": "1815.ppd.gz"
      } ]
    }
  }
})";

  const auto parsed = ParseForwardIndex(kJsonForwardIndex);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_THAT(parsed.value(),
              UnorderedElementsAre(ParsedIndexEntryLike(
                  "rastlose liebe",
                  UnorderedElementsAre(
                      ParsedIndexLeafWithPpdBasename("d-138.ppd.gz"),
                      ParsedIndexLeafWithPpdBasename("1815.ppd.gz")))));
}

// Verifies that ParseForwardIndex() can parse forward index metadata
// and its well-formed restrictions (if any are specified).
TEST(PpdMetadataParserTest, CanParseForwardIndexWithRestrictions) {
  // Specifies
  // *  a PPD metadata leaf with a minimum milestone,
  // *  a PPD metadata leaf with a maximum milestone, and
  // *  a PPD metadata leaf with both minimum and maximum milestones.
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "nähe des geliebten": {
      "ppdMetadata": [ {
        "name": "d-162.ppd.gz",
        "restriction": {
          "minMilestone": 25
        }
      } ]
    },
    "der fischer": {
      "ppdMetadata": [ {
        "name": "d-225.ppd.gz",
        "restriction": {
          "maxMilestone": 35
        }
      } ]
    },
    "erster verlust": {
      "ppdMetadata": [ {
        "name": "d-226.ppd.gz",
        "restriction": {
          "minMilestone": 45,
          "maxMilestone": 46
        }
      } ]
    }
  }
})";

  const auto parsed = ParseForwardIndex(kJsonForwardIndex);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_THAT(
      parsed.value(),
      UnorderedElementsAre(
          ParsedIndexEntryLike(
              "nähe des geliebten",
              UnorderedElementsAre(
                  AllOf(ParsedIndexLeafWithPpdBasename("d-162.ppd.gz"),
                        Field(&ParsedIndexLeaf::restrictions,
                              RestrictionsWithMinMilestone(25))))),
          ParsedIndexEntryLike(
              "der fischer", UnorderedElementsAre(AllOf(
                                 ParsedIndexLeafWithPpdBasename("d-225.ppd.gz"),
                                 Field(&ParsedIndexLeaf::restrictions,
                                       RestrictionsWithMaxMilestone(35))))),
          ParsedIndexEntryLike(
              "erster verlust",
              UnorderedElementsAre(
                  AllOf(ParsedIndexLeafWithPpdBasename("d-226.ppd.gz"),
                        Field(&ParsedIndexLeaf::restrictions,
                              RestrictionsWithMinAndMaxMilestones(45, 46)))))));
}

// Verifies that ParseForwardIndex() can parse forward index metadata
// and ignore malformed restrictions.
TEST(PpdMetadataParserTest, CanParseForwardIndexWithMalformedRestrictions) {
  // Same test data as the CanParseForwardIndexWithRestrictions test
  // above, but defines
  // *  a PPD metadata leaf with a malformed minimum milestone,
  // *  a PPD metadata leaf with a malformed maximum milestone, and
  // *  a PPD metadata leaf with malformed minimum and maximum
  //    milestones.
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "nähe des geliebten": {
      "ppdMetadata": [ {
        "name": "d-162.ppd.gz",
        "restriction": {
          "minMilestone": "garbage value",
          "maxMilestone": 25
        }
      } ]
    },
    "der fischer": {
      "ppdMetadata": [ {
        "name": "d-225.ppd.gz",
        "restriction": {
          "minMilestone": 35,
          "maxMilestone": "garbage value"
        }
      } ]
    },
    "erster verlust": {
      "ppdMetadata": [ {
        "name": "d-226.ppd.gz",
        "restriction": {
          "minMilestone": "garbage value",
          "maxMilestone": "garbage value"
        }
      } ]
    }
  }
})";

  const auto parsed = ParseForwardIndex(kJsonForwardIndex);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_THAT(
      parsed.value(),
      UnorderedElementsAre(
          ParsedIndexEntryLike(
              "nähe des geliebten",
              UnorderedElementsAre(
                  AllOf(ParsedIndexLeafWithPpdBasename("d-162.ppd.gz"),
                        Field(&ParsedIndexLeaf::restrictions,
                              RestrictionsWithMaxMilestone(25))))),
          ParsedIndexEntryLike(
              "der fischer", UnorderedElementsAre(AllOf(
                                 ParsedIndexLeafWithPpdBasename("d-225.ppd.gz"),
                                 Field(&ParsedIndexLeaf::restrictions,
                                       RestrictionsWithMinMilestone(35))))),
          ParsedIndexEntryLike(
              "erster verlust",
              UnorderedElementsAre(
                  AllOf(ParsedIndexLeafWithPpdBasename("d-226.ppd.gz"),
                        Field(&ParsedIndexLeaf::restrictions,
                              UnboundedRestrictions()))))));
}

// Verifies that ParseForwardIndex() can parse forward index metadata
// specifying PPD licenses.
TEST(PpdMetadataParserTest, CanParseForwardIndexWithLicenses) {
  // Specifies two PPDs, each with a license associated.
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "der fischer": {
      "ppdMetadata": [ {
        "name": "d-225.ppd.gz",
        "license": "two two five license"
      } ]
    },
    "erster verlust": {
      "ppdMetadata": [ {
        "name": "d-226.ppd.gz",
        "license": "two two six license"
      } ]
    }
  }
})";
  const auto parsed = ParseForwardIndex(kJsonForwardIndex);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_THAT(
      parsed.value(),
      UnorderedElementsAre(
          ParsedIndexEntryLike(
              "der fischer", UnorderedElementsAre(AllOf(
                                 ParsedIndexLeafWithPpdBasename("d-225.ppd.gz"),
                                 Field(&ParsedIndexLeaf::license,
                                       StrEq("two two five license"))))),
          ParsedIndexEntryLike(
              "erster verlust",
              UnorderedElementsAre(
                  AllOf(ParsedIndexLeafWithPpdBasename("d-226.ppd.gz"),
                        Field(&ParsedIndexLeaf::license,
                              StrEq("two two six license")))))));
}

// Verifies that ParseForwardIndex() returns absl::nullopt rather than
// an empty container.
TEST(PpdMetadataParserTest, ParseForwardIndexDoesNotReturnEmptyContainer) {
  // Specifies a forward index that is valid JSON but which has
  // no PPDs whose leaf values are non-empty.
  constexpr base::StringPiece kJsonForwardIndex = R"({
  "ppdIndex": {
    "der könig in thule": {
      "ppdMetadata": [ {
        "garbage": "unused value"
      } ]
    }
  }
})";

  EXPECT_FALSE(ParseForwardIndex(kJsonForwardIndex).has_value());
}

// Verifies that ParseForwardIndex() returns absl::nullopt on
// irrecoverable parse error.
TEST(PpdMetadataParserTest, ParseForwardIndexFailsGracefully) {
  EXPECT_FALSE(ParseForwardIndex(kInvalidJson).has_value());
}

// Verifies that ParseUsbIndex() can parse USB index metadata.
TEST(PpdMetadataParserTest, CanParseUsbIndex) {
  constexpr base::StringPiece kJsonUsbIndex = R"({
  "usbIndex": {
    "1": {
      "effectiveMakeAndModel": "d 541"
    },
    "10": {
      "effectiveMakeAndModel": "d 504"
    }
  }
})";

  EXPECT_THAT(ParseUsbIndex(kJsonUsbIndex),
              Optional(UnorderedElementsAre(Pair(1, StrEq("d 541")),
                                            Pair(10, StrEq("d 504")))));
}

// Verifies that ParseUsbIndex() can parse USB index metadata and return
// a partial ParsedUsbIndex even when it encounters garbage values.
TEST(PpdMetadataParserTest, CanPartiallyParseUsbIndex) {
  constexpr base::StringPiece kJsonUsbIndex = R"({
  "usbIndex": {
    "garbage key": {
      "effectiveMakeAndModel": "garbage value"
    },
    "1": {
      "effectiveMakeAndModel": "d 541"
    },
    "10": {
      "effectiveMakeAndModel": "d 504"
    }
  }
})";

  EXPECT_THAT(ParseUsbIndex(kJsonUsbIndex),
              Optional(UnorderedElementsAre(Pair(1, StrEq("d 541")),
                                            Pair(10, StrEq("d 504")))));
}

// Verifies that ParseUsbIndex() returns absl::nullopt rather than an
// empty container.
TEST(PpdMetadataParserTest, ParseUsbIndexDoesNotReturnEmptyContainer) {
  constexpr base::StringPiece kEmptyJsonUsbIndex = R"({
  "usbIndex": { }
})";
  ASSERT_THAT(base::JSONReader::Read(kEmptyJsonUsbIndex), Ne(absl::nullopt));
  EXPECT_THAT(ParseUsbIndex(kEmptyJsonUsbIndex), Eq(absl::nullopt));

  constexpr base::StringPiece kJsonUsbIndexWithBadStringKeys = R"({
  "usbIndex": {
    "non-integral key": { }
  }
})";
  ASSERT_THAT(base::JSONReader::Read(kJsonUsbIndexWithBadStringKeys),
              Ne(absl::nullopt));
  EXPECT_THAT(ParseUsbIndex(kJsonUsbIndexWithBadStringKeys), Eq(absl::nullopt));

  constexpr base::StringPiece kJsonUsbIndexWithoutEmmAtLeaf = R"({
  "usbIndex": {
    "1": {
      "some key that is not ``effectiveMakeAndModel''": "d 504"
    }
  }
})";
  ASSERT_THAT(base::JSONReader::Read(kJsonUsbIndexWithoutEmmAtLeaf),
              Ne(absl::nullopt));
  EXPECT_THAT(ParseUsbIndex(kJsonUsbIndexWithoutEmmAtLeaf), Eq(absl::nullopt));

  constexpr base::StringPiece kJsonUsbIndexWithEmptyEmmAtLeaf = R"({
  "usbIndex": {
    "1": {
      "effectiveMakeAndModel": ""
    }
  }
})";
  ASSERT_THAT(base::JSONReader::Read(kJsonUsbIndexWithEmptyEmmAtLeaf),
              Ne(absl::nullopt));
  EXPECT_THAT(ParseUsbIndex(kJsonUsbIndexWithEmptyEmmAtLeaf),
              Eq(absl::nullopt));
}

// Verifies that ParseUsbIndex() returns absl::nullopt on irrecoverable
// parse error.
TEST(PpdMetadataParserTest, ParseUsbIndexFailsGracefully) {
  EXPECT_THAT(ParseUsbIndex(kInvalidJson), Eq(absl::nullopt));
}

// Verifies that ParseUsbvendorIdMap() can parse USB vendor ID maps.
TEST(PpdMetadataParserTest, CanParseUsbVendorIdMap) {
  constexpr base::StringPiece kJsonUsbVendorIdMap = R"({
  "entries": [ {
    "vendorId": 1111,
    "vendorName": "One One One One"
  }, {
    "vendorId": 2222,
    "vendorName": "Two Two Two Two"
  }, {
    "vendorId": 3333,
    "vendorName": "Three Three Three Three"
  } ]
})";

  EXPECT_THAT(ParseUsbVendorIdMap(kJsonUsbVendorIdMap),
              Optional(UnorderedElementsAre(
                  Pair(1111, StrEq("One One One One")),
                  Pair(2222, StrEq("Two Two Two Two")),
                  Pair(3333, StrEq("Three Three Three Three")))));
}

// Verifies that ParseUsbvendorIdMap() can parse USB vendor ID maps and
// return partial results even when it encounters garbage values.
TEST(PpdMetadataParserTest, CanPartiallyParseUsbVendorIdMap) {
  // This USB vendor ID map has garbage values in it.
  // ParseUsbVendorIdMap() shall ignore these.
  constexpr base::StringPiece kJsonUsbVendorIdMap = R"({
  "garbage key": "garbage value",
  "entries": [
    "garbage value",
  {
    "vendorId": 1111,
    "garbage key": "garbage value",
    "vendorName": "One One One One"
  }, {
    "vendorId": 2222,
    "vendorName": "Two Two Two Two"
  }, {
    "vendorId": 3333,
    "vendorName": "Three Three Three Three"
  } ]
})";

  EXPECT_THAT(ParseUsbVendorIdMap(kJsonUsbVendorIdMap),
              Optional(UnorderedElementsAre(
                  Pair(1111, StrEq("One One One One")),
                  Pair(2222, StrEq("Two Two Two Two")),
                  Pair(3333, StrEq("Three Three Three Three")))));
}

// Verifies that ParseUsbvendorIdMap() returns absl::nullopt rather
// than an empty container.
TEST(PpdMetadataParserTest, ParseUsbVendorIdMapDoesNotReturnEmptyContainer) {
  // Defines a USB vendor ID map that is empty; it's valid JSON, but
  // has no values worth returning.
  constexpr base::StringPiece kJsonUsbVendorIdMap = "{}";

  EXPECT_THAT(ParseUsbVendorIdMap(kJsonUsbVendorIdMap), Eq(absl::nullopt));
}

// Verifies that ParseUsbvendorIdMap() returns absl::nullopt on
// irrecoverable parse error.
TEST(PpdMetadataParserTest, ParseUsbVendorIdMapFailsGracefully) {
  EXPECT_THAT(ParseUsbVendorIdMap(kInvalidJson), Eq(absl::nullopt));
}

// Verifies that ParseReverseIndex() can parse reverse index metadata.
TEST(PpdMetadataParserTest, CanParseReverseIndex) {
  constexpr base::StringPiece kReverseIndexJson = R"(
  {
    "reverseIndex": {
      "Die Forelle D 550d": {
        "manufacturer": "metsukabi",
        "model": "kimebe"
      },
      "Gruppe aus dem Tartarus D 583": {
        "manufacturer": "teiga",
        "model": "dahuho"
      }
    }
  }
  )";

  const auto parsed = ParseReverseIndex(kReverseIndexJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed, UnorderedElementsAre(
                           Pair(StrEq("Die Forelle D 550d"),
                                ReverseIndexLeafLike("metsukabi", "kimebe")),
                           Pair(StrEq("Gruppe aus dem Tartarus D 583"),
                                ReverseIndexLeafLike("teiga", "dahuho"))));
}

// Verifies that ParseReverseIndex() can parse reverse index metadata
// and return a partial list even when it encounters unexpected values.
TEST(PpdMetadataParserTest, CanPartiallyParseReverseIndex) {
  // Contains two unexpected values (keyed on "Dearie me" and "to go").
  // ParseReverseIndex() shall ignore these.
  constexpr base::StringPiece kReverseIndexJson = R"(
  {
    "reverseIndex": {
      "Dearie me": "one doesn't expect",
      "to go": "any deeper",
      "Elysium D 584": {
        "manufacturer": "nahopenu",
        "model": "sapudo"
      },
      "An den Tod D 518": {
        "manufacturer": "suwaka",
        "model": "zogegi"
      }
    }
  }
  )";

  const auto parsed = ParseReverseIndex(kReverseIndexJson);
  ASSERT_TRUE(parsed.has_value());

  EXPECT_THAT(*parsed, UnorderedElementsAre(
                           Pair(StrEq("Elysium D 584"),
                                ReverseIndexLeafLike("nahopenu", "sapudo")),
                           Pair(StrEq("An den Tod D 518"),
                                ReverseIndexLeafLike("suwaka", "zogegi"))));
}

// Verifies that ParseReverseIndex() returns absl::nullopt rather than
// an empty container.
TEST(PpdMetadataParserTest, ParseReverseIndexDoesNotReturnEmptyContainer) {
  // Contains two unexpected values (keyed on "Dearie me" and "to go").
  // ParseReverseIndex() shall ignore this, but in doing so shall make the
  // returned ParsedReverseIndex empty.
  constexpr base::StringPiece kReverseIndexJson = R"(
  {
    "reverseIndex": {
      "Dearie me": "one doesn't expect",
      "to go": "any deeper"
    }
  }
  )";

  EXPECT_FALSE(ParseReverseIndex(kReverseIndexJson).has_value());
}

// Verifies that ParseReverseIndex() returns absl::nullopt on
// irrecoverable parse error.
TEST(PpdMetadataParserTest, ParseReverseIndexFailsGracefully) {
  EXPECT_FALSE(ParseReverseIndex(kInvalidJson).has_value());
}

}  // namespace
}  // namespace chromeos
