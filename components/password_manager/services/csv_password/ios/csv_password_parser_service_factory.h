// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_IOS_CSV_PASSWORD_PARSER_SERVICE_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_IOS_CSV_PASSWORD_PARSER_SERVICE_FACTORY_H_

#include "components/password_manager/services/csv_password/public/mojom/csv_password_parser.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace password_manager {

class CSVPasswordParserImpl;

// A singleton class used on iOS to hold the CSVPasswordParserImpl object alive
// while the csv password parser service is running.
class CSVPasswordParserServiceFactory {
 public:
  static CSVPasswordParserServiceFactory* GetInstance();

  CSVPasswordParserServiceFactory(const CSVPasswordParserServiceFactory&) =
      delete;
  CSVPasswordParserServiceFactory& operator=(
      const CSVPasswordParserServiceFactory&) = delete;

  mojo::Remote<password_manager::mojom::CSVPasswordParser>
  LaunchCSVPasswordParser();

 private:
  friend struct base::DefaultSingletonTraits<CSVPasswordParserServiceFactory>;

  CSVPasswordParserServiceFactory();
  ~CSVPasswordParserServiceFactory();

  std::unique_ptr<CSVPasswordParserImpl> parser_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_SERVICES_CSV_PASSWORD_IOS_CSV_PASSWORD_PARSER_SERVICE_FACTORY_H_
