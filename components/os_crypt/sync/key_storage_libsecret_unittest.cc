// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unordered_map>

#include "base/lazy_instance.h"
#include "components/os_crypt/sync/key_storage_libsecret.h"
#include "components/os_crypt/sync/libsecret_util_linux.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/glib/scoped_gobject.h"

namespace {

const SecretSchema kKeystoreSchemaV2 = {
    "chrome_libsecret_os_crypt_password_v2",
    SECRET_SCHEMA_DONT_MATCH_NAME,
    {
        {"application", SECRET_SCHEMA_ATTRIBUTE_STRING},
        {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
    }};

// This test mocks C-functions used by Libsecret. In order to present a
// consistent world view, we need a single backing instance that contains all
// the relevant data.
class MockPasswordStore {
  // The functions that interact with the password store expect to return
  // gobjects. These C-style objects are hard to work with. Rather than finagle
  // with the type system, we always use objects with type G_TYPE_OBJECT. We
  // then keep a local map from G_TYPE_OBJECT to std::string, and all relevant
  // translation just look up entries in this map.
 public:
  void Reset() {
    mapping_.clear();
    ClearPassword();
    for (GObject* o : objects_returned_to_caller_) {
      ASSERT_EQ(o->ref_count, 1u);
    }
    objects_returned_to_caller_.clear();
  }

  void ClearPassword() {
    if (password_) {
      ASSERT_EQ((*password_).ref_count, 1u);
      password_ = ScopedGObject<GObject>();  // Reset
    }
  }

  void SetPassword(const std::string& password) {
    ASSERT_FALSE(password_);
    password_ = TakeGObject(
        static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr)));
    mapping_[password_.get()] = password;
  }

  // The returned object has a ref count of 2. This way, after the client
  // deletes the object, it isn't destroyed, and we can check that all these
  // objects have ref count of 1 at the end of the test.
  GObject* MakeTempObject(const std::string& value) {
    ScopedGObject temp = WrapGObject(
        static_cast<GObject*>(g_object_new(G_TYPE_OBJECT, nullptr)));
    objects_returned_to_caller_.push_back(temp);
    mapping_[temp.get()] = value;
    return temp;
  }

  const gchar* GetString(void* opaque_id) {
    return mapping_[static_cast<GObject*>(opaque_id)].c_str();
  }

  GObject* password() { return password_; }

  // The keys in `mapping_` refer to objects in `objects_returned_to_caller_` or
  // `password_`, which manage their lifetime.
  std::unordered_map<GObject*, std::string> mapping_;
  std::vector<ScopedGObject<GObject>> objects_returned_to_caller_;
  ScopedGObject<GObject> password_;
};
base::LazyInstance<MockPasswordStore>::Leaky g_password_store =
    LAZY_INSTANCE_INITIALIZER;

// Replaces some of LibsecretLoader's methods with mocked ones.
class MockLibsecretLoader : public LibsecretLoader {
 public:
  // Sets up the minimum mock implementation necessary for OSCrypt to work
  // with Libsecret. Also resets the state to mock a clean database.
  static bool ResetForOSCrypt();

  // Sets OSCrypt's password in the libsecret mock to a specific value
  static void SetOSCryptPassword(const char*);

  // Releases memory and restores LibsecretLoader to an uninitialized state.
  static void TearDown();

 private:
  // These methods are used to redirect calls through LibsecretLoader
  static const gchar* mock_secret_value_get_text(SecretValue* value);

  static gboolean mock_secret_password_store_sync(const SecretSchema* schema,
                                                  const gchar* collection,
                                                  const gchar* label,
                                                  const gchar* password,
                                                  GCancellable* cancellable,
                                                  GError** error,
                                                  ...);

  static void mock_secret_value_unref(gpointer value);

  static GList* mock_secret_service_search_sync(SecretService* service,
                                                const SecretSchema* schema,
                                                GHashTable* attributes,
                                                SecretSearchFlags flags,
                                                GCancellable* cancellable,
                                                GError** error);

  static SecretValue* mock_secret_item_get_secret(SecretItem* item);

  static guint64 mock_secret_item_get_created(SecretItem* item);

  static guint64 mock_secret_item_get_modified(SecretItem* item);
};

const gchar* MockLibsecretLoader::mock_secret_value_get_text(
    SecretValue* value) {
  return g_password_store.Pointer()->GetString(value);
}

// static
gboolean MockLibsecretLoader::mock_secret_password_store_sync(
    const SecretSchema* schema,
    const gchar* collection,
    const gchar* label,
    const gchar* password,
    GCancellable* cancellable,
    GError** error,
    ...) {
  // TODO(crbug.com/40490926) We don't read the dummy we store to unlock
  // keyring.
  if (strcmp("_chrome_dummy_schema_for_unlocking", schema->name) == 0) {
    return true;
  }

  EXPECT_STREQ(kKeystoreSchemaV2.name, schema->name);
  g_password_store.Pointer()->SetPassword(password);
  return true;
}

// static
void MockLibsecretLoader::mock_secret_value_unref(gpointer value) {
  g_object_unref(value);
}

// static
GList* MockLibsecretLoader::mock_secret_service_search_sync(
    SecretService* service,
    const SecretSchema* schema,
    GHashTable* attributes,
    SecretSearchFlags flags,
    GCancellable* cancellable,
    GError** error) {
  EXPECT_STREQ(kKeystoreSchemaV2.name, schema->name);

  EXPECT_TRUE(flags & SECRET_SEARCH_UNLOCK);
  EXPECT_TRUE(flags & SECRET_SEARCH_LOAD_SECRETS);

  GObject* item = nullptr;
  MockPasswordStore* store = g_password_store.Pointer();
  GObject* password = store->password();
  if (password) {
    item = store->MakeTempObject(store->GetString(password));
  }

  if (!item) {
    return nullptr;
  }

  GList* result = nullptr;
  result = g_list_append(result, item);
  g_clear_error(error);
  return result;
}

// static
SecretValue* MockLibsecretLoader::mock_secret_item_get_secret(
    SecretItem* item) {
  // Add a ref to make sure that the caller unrefs with secret_value_unref.
  g_object_ref(item);
  return reinterpret_cast<SecretValue*>(item);
}

// static
guint64 MockLibsecretLoader::mock_secret_item_get_created(SecretItem* item) {
  return 0;
}

// static
guint64 MockLibsecretLoader::mock_secret_item_get_modified(SecretItem* item) {
  return 0;
}

// static
bool MockLibsecretLoader::ResetForOSCrypt() {
  // Methods used by KeyStorageLibsecret
  secret_password_store_sync =
      &MockLibsecretLoader::mock_secret_password_store_sync;
  secret_value_get_text = &MockLibsecretLoader::mock_secret_value_get_text;
  secret_value_unref = &MockLibsecretLoader::mock_secret_value_unref;
  secret_service_search_sync =
      &MockLibsecretLoader::mock_secret_service_search_sync;
  secret_item_get_secret = &MockLibsecretLoader::mock_secret_item_get_secret;
  secret_item_get_created = &MockLibsecretLoader::mock_secret_item_get_created;
  secret_item_get_modified =
      &MockLibsecretLoader::mock_secret_item_get_modified;

  g_password_store.Pointer()->Reset();
  libsecret_loaded_ = true;

  return true;
}

// static
void MockLibsecretLoader::TearDown() {
  g_password_store.Pointer()->Reset();
  libsecret_loaded_ =
      false;  // Function pointers will be restored when loading.
}

class LibsecretTest : public testing::Test {
 public:
  LibsecretTest() = default;

  LibsecretTest(const LibsecretTest&) = delete;
  LibsecretTest& operator=(const LibsecretTest&) = delete;

  ~LibsecretTest() override = default;

  void SetUp() override { MockLibsecretLoader::ResetForOSCrypt(); }

  void TearDown() override { MockLibsecretLoader::TearDown(); }
};

TEST_F(LibsecretTest, LibsecretRepeats) {
  KeyStorageLibsecret libsecret("chromium");
  MockLibsecretLoader::ResetForOSCrypt();
  g_password_store.Pointer()->SetPassword("initial password");
  std::optional<std::string> password = libsecret.GetKey();
  EXPECT_TRUE(password.has_value());
  EXPECT_FALSE(password.value().empty());
  std::optional<std::string> password_repeat = libsecret.GetKey();
  EXPECT_TRUE(password_repeat.has_value());
  EXPECT_EQ(password.value(), password_repeat.value());
}

TEST_F(LibsecretTest, LibsecretCreatesRandomised) {
  KeyStorageLibsecret libsecret("chromium");
  MockLibsecretLoader::ResetForOSCrypt();
  std::optional<std::string> password = libsecret.GetKey();
  MockLibsecretLoader::ResetForOSCrypt();
  std::optional<std::string> password_new = libsecret.GetKey();
  EXPECT_TRUE(password.has_value());
  EXPECT_TRUE(password_new.has_value());
  EXPECT_NE(password.value(), password_new.value());
}

}  // namespace
