// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/utility/importer/edge_database_reader_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace {

class EdgeDatabaseReaderTest : public ::testing::Test {
 protected:
  bool CopyTestDatabase(const std::wstring& database_name,
                        base::FilePath* copied_path) {
    base::FilePath input_path;
    input_path = test_data_path_.AppendASCII("edge_database_reader")
                     .Append(database_name)
                     .AddExtension(L".gz");
    base::FilePath output_path = temp_dir_.GetPath().Append(database_name);

    if (DecompressDatabase(input_path, output_path)) {
      *copied_path = output_path;
      return true;
    }
    return false;
  }

  bool WriteFile(const std::wstring& name,
                 const std::string& contents,
                 base::FilePath* output_path) {
    *output_path = temp_dir_.GetPath().Append(name);
    return base::WriteFile(*output_path, contents);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path_));
  }

 private:
  bool DecompressDatabase(const base::FilePath& gzip_file,
                          const base::FilePath& output_file) {
    std::string gzip_data;
    if (!base::ReadFileToString(gzip_file, &gzip_data))
      return false;
    if (!compression::GzipUncompress(gzip_data, &gzip_data))
      return false;
    return base::WriteFile(output_file, gzip_data);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath test_data_path_;
};

}  // namespace

TEST_F(EdgeDatabaseReaderTest, OpenFileTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
}

TEST_F(EdgeDatabaseReaderTest, NoFileTest) {
  base::FilePath database_path(L"ThisIsntARealFileName.edb");
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path));
}

TEST_F(EdgeDatabaseReaderTest, RandomGarbageDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"random.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path));
}

TEST_F(EdgeDatabaseReaderTest, ZerosDatabaseTest) {
  base::FilePath database_path;
  std::string zeros(0x10000, '\0');
  ASSERT_TRUE(WriteFile(L"zeros.edb", zeros, &database_path));
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path));
}

TEST_F(EdgeDatabaseReaderTest, EmptyDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(WriteFile(L"empty.edb", "", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_FALSE(reader.OpenDatabase(database_path));
}

TEST_F(EdgeDatabaseReaderTest, OpenTableDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(nullptr, table_enum);
}

TEST_F(EdgeDatabaseReaderTest, InvalidTableDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NotARealTableName");
  EXPECT_EQ(nullptr, table_enum);
}

TEST_F(EdgeDatabaseReaderTest, NotOpenDatabaseTest) {
  EdgeDatabaseReader reader;
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_EQ(nullptr, table_enum);
  EXPECT_EQ(JET_errDatabaseNotFound, reader.last_error());
}

TEST_F(EdgeDatabaseReaderTest, AlreadyOpenDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  EXPECT_FALSE(reader.OpenDatabase(database_path));
  EXPECT_EQ(JET_errOneDatabasePerSession, reader.last_error());
}

TEST_F(EdgeDatabaseReaderTest, OpenTableAndReadDataDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(nullptr, table_enum);
  int row_count = 0;
  do {
    int32_t int_col_value = 0;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
    EXPECT_EQ(-row_count, int_col_value);

    uint32_t uint_col_value = 0;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"UIntCol", &uint_col_value));
    EXPECT_EQ(static_cast<uint32_t>(row_count), uint_col_value);

    int64_t longlong_col_value = 0;
    EXPECT_TRUE(
        table_enum->RetrieveColumn(L"LongLongCol", &longlong_col_value));
    EXPECT_EQ(row_count, longlong_col_value);

    GUID guid_col_value = {};
    GUID expected_guid_col_value = {};
    EXPECT_TRUE(table_enum->RetrieveColumn(L"GuidCol", &guid_col_value));
    memset(&expected_guid_col_value, row_count,
           sizeof(expected_guid_col_value));
    EXPECT_EQ(expected_guid_col_value, guid_col_value);

    FILETIME filetime_col_value = {};
    FILETIME expected_filetime_col_value = {};
    SYSTEMTIME system_time = {};
    // Expected time value is row_count+1/January/1970.
    system_time.wYear = 1970;
    system_time.wMonth = 1;
    system_time.wDay = row_count + 1;
    EXPECT_TRUE(
        SystemTimeToFileTime(&system_time, &expected_filetime_col_value));
    EXPECT_TRUE(table_enum->RetrieveColumn(L"DateCol", &filetime_col_value));
    EXPECT_EQ(expected_filetime_col_value.dwLowDateTime,
              filetime_col_value.dwLowDateTime);
    EXPECT_EQ(expected_filetime_col_value.dwHighDateTime,
              filetime_col_value.dwHighDateTime);

    std::u16string row_string =
        base::AsString16(base::StringPrintf(L"String: %d", row_count));
    std::u16string str_col_value;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
    EXPECT_EQ(row_string, str_col_value);

    bool bool_col_value;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"BoolCol", &bool_col_value));
    EXPECT_EQ((row_count % 2) == 0, bool_col_value);

    row_count++;
  } while (table_enum->Next());
  EXPECT_EQ(16, row_count);
}

TEST_F(EdgeDatabaseReaderTest, CheckEnumResetDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(nullptr, table_enum);
  int row_count = 0;
  do {
    row_count++;
  } while (table_enum->Next());
  EXPECT_NE(0, row_count);
  EXPECT_TRUE(table_enum->Reset());
  do {
    row_count--;
  } while (table_enum->Next());
  EXPECT_EQ(0, row_count);
}

TEST_F(EdgeDatabaseReaderTest, InvalidColumnDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(nullptr, table_enum);
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"NotARealNameCol", &int_col_value));
  EXPECT_EQ(JET_errColumnNotFound, table_enum->last_error());
}

TEST_F(EdgeDatabaseReaderTest, NoColumnDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NoColsTable");
  EXPECT_NE(nullptr, table_enum);
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
  EXPECT_EQ(JET_errColumnNotFound, table_enum->last_error());
}

TEST_F(EdgeDatabaseReaderTest, EmptyTableDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"EmptyTable");
  EXPECT_NE(nullptr, table_enum);
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
  EXPECT_NE(JET_errColumnNotFound, table_enum->last_error());
  EXPECT_FALSE(table_enum->Reset());
  EXPECT_FALSE(table_enum->Next());
}

TEST_F(EdgeDatabaseReaderTest, UnicodeStringsDatabaseTest) {
  const char* utf8_strings[] = {
      "\xE3\x81\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF",
      "\xE4\xBD\xA0\xE5\xA5\xBD",
      ("\xD0\x97\xD0\xB4\xD1\x80\xD0\xB0\xD0\xB2\xD1\x81\xD1\x82\xD0\xB2\xD1"
       "\x83\xD0\xB9\xD1\x82\xD0\xB5"),
      "\x48\x65\x6C\x6C\x6F",
      "\xEC\x95\x88\xEB\x85\x95\xED\x95\x98\xEC\x84\xB8\xEC\x9A\x94",
  };

  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"UnicodeTable");
  EXPECT_NE(nullptr, table_enum);
  size_t utf8_strings_count = std::size(utf8_strings);
  for (size_t row_count = 0; row_count < utf8_strings_count; ++row_count) {
    std::u16string row_string = base::UTF8ToUTF16(utf8_strings[row_count]);
    std::u16string str_col_value;
    EXPECT_TRUE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
    EXPECT_EQ(row_string, str_col_value);
    if (row_count < utf8_strings_count - 1)
      EXPECT_TRUE(table_enum->Next());
    else
      EXPECT_FALSE(table_enum->Next());
  }
}

TEST_F(EdgeDatabaseReaderTest, NonUnicodeStringsDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NonUnicodeTable");
  EXPECT_NE(nullptr, table_enum);
  std::u16string str_col_value;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
}

TEST_F(EdgeDatabaseReaderTest, CheckNullColumnDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"NullTable");
  EXPECT_NE(nullptr, table_enum);

  // We expect to successfully open a column value but get the default value
  // back.
  int32_t int_col_value = 1;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"IntCol", &int_col_value));
  EXPECT_EQ(0, int_col_value);

  uint32_t uint_col_value = 1;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"UIntCol", &uint_col_value));
  EXPECT_EQ(0u, uint_col_value);

  int64_t longlong_col_value = 1;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"LongLongCol", &longlong_col_value));
  EXPECT_EQ(0, longlong_col_value);

  GUID guid_col_value = {};
  GUID expected_guid_col_value = {};
  memset(&guid_col_value, 0x1, sizeof(guid_col_value));
  EXPECT_TRUE(table_enum->RetrieveColumn(L"GuidCol", &guid_col_value));
  memset(&expected_guid_col_value, 0, sizeof(expected_guid_col_value));
  EXPECT_EQ(expected_guid_col_value, guid_col_value);

  FILETIME filetime_col_value = {1, 1};
  FILETIME expected_filetime_col_value = {};
  EXPECT_TRUE(table_enum->RetrieveColumn(L"DateCol", &filetime_col_value));
  EXPECT_EQ(expected_filetime_col_value.dwLowDateTime,
            filetime_col_value.dwLowDateTime);
  EXPECT_EQ(expected_filetime_col_value.dwHighDateTime,
            filetime_col_value.dwHighDateTime);

  std::u16string str_col_value;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"StrCol", &str_col_value));
  EXPECT_TRUE(str_col_value.empty());

  bool bool_col_value;
  EXPECT_TRUE(table_enum->RetrieveColumn(L"BoolCol", &bool_col_value));
  EXPECT_EQ(false, bool_col_value);
}

TEST_F(EdgeDatabaseReaderTest, CheckInvalidColumnTypeDatabaseTest) {
  base::FilePath database_path;
  ASSERT_TRUE(CopyTestDatabase(L"testdata.edb", &database_path));
  EdgeDatabaseReader reader;
  EXPECT_TRUE(reader.OpenDatabase(database_path));
  std::unique_ptr<EdgeDatabaseTableEnumerator> table_enum =
      reader.OpenTableEnumerator(L"TestTable");
  EXPECT_NE(nullptr, table_enum);

  uint32_t uint_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"IntCol", &uint_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
  // Check unsigned int with a signed int.
  int32_t int_col_value = 0;
  EXPECT_FALSE(table_enum->RetrieveColumn(L"UIntCol", &int_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
  EXPECT_FALSE(table_enum->RetrieveColumn(L"LongLongCol", &uint_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
  EXPECT_FALSE(table_enum->RetrieveColumn(L"GuidCol", &uint_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
  EXPECT_FALSE(table_enum->RetrieveColumn(L"DateCol", &uint_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
  EXPECT_FALSE(table_enum->RetrieveColumn(L"StrCol", &uint_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
  EXPECT_FALSE(table_enum->RetrieveColumn(L"BoolCol", &uint_col_value));
  EXPECT_EQ(JET_errInvalidColumnType, table_enum->last_error());
}
