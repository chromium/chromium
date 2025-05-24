// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Foundation

enum FieldLabel: String {
  case origin, username, password, note
}

@objcMembers public class PasswordData: NSObject {
  public var origin: String
  public var username: String
  public var password: String
  public var note: String?

  init(
    row_origin: String,
    row_username: String,
    row_password: String,
    row_note: String?
  ) {
    origin = row_origin
    username = row_username
    password = row_password
    note = row_note
  }
}

@objc public class CSVPasswordsParser: NSObject {
  @objc public var passwords: [PasswordData]?

  @objc public static func fromCSVInput(_ CSVInput: String?)
    -> CSVPasswordsParser?
  {
    guard let input = CSVInput else {
      return nil
    }
    return CSVPasswordsParser(input: input)
  }

  init(input: String) {
    passwords = CSVPasswordsParser.parse(input: input)
    super.init()
  }

  // Returns the first row from the input.
  // The first row is removed from the input.
  static func consumeCSVLine(input: inout String) -> String {
    if input.isEmpty {
      return ""
    }

    var insideQuotes = false
    var lastCharWasCR = false
    var currentIndex = input.startIndex

    while currentIndex < input.endIndex {
      let currentChar = input[currentIndex]

      switch currentChar {
      case "\n", "\r\n":
        if insideQuotes {
          break
        }

        let eolStart =
          lastCharWasCR ? input.index(before: currentIndex) : currentIndex
        let ret = String(input[..<eolStart])
        input = String(input[input.index(after: currentIndex)...])
        return ret
      case "\"":
        insideQuotes.toggle()
        break
      default:
        break
      }

      lastCharWasCR = (currentChar == "\r")
      currentIndex = input.index(after: currentIndex)
    }

    // The rest of the input is the last line.
    let ret = input
    input = ""
    return ret
  }

  // Removes the leading whitespaces.
  static func trimLeadingWhitespaceAndNewlines(string: String) -> String {
    var startIndex = string.startIndex

    while startIndex < string.endIndex {
      let currentChar = string[startIndex]
      if currentChar == "\r" || currentChar == " " || currentChar == "\t" {
        startIndex = string.index(after: startIndex)
      } else {
        break
      }
    }

    return String(string[startIndex...])
  }

  // Returns the next valid row.
  static func seekToNextValidRow(string: inout String) -> String {
    var row = ""
    repeat {
      row = trimLeadingWhitespaceAndNewlines(
        string: consumeCSVLine(input: &string))
    } while row.isEmpty && !string.isEmpty
    return row
  }

  // Returns the priority and label for the input.
  static func label(forFieldName input: String) -> (Int, FieldLabel?) {
    let normalizedInput = input.trimmingCharacters(
      in: .whitespacesAndNewlines
    ).lowercased()

    switch normalizedInput {
    case "url", "website", "origin", "hostname", "login_uri":
      return (1, .origin)
    case "username", "user", "login", "account", "login_username":
      return (1, .username)
    case "password", "login_password":
      return (1, .password)
    case "note":
      return (4, .note)
    case "notes":
      return (3, .note)
    case "comment":
      return (2, .note)
    case "comments":
      return (1, .note)
    default:
      return (0, nil)  // Return nil if no match is found
    }
  }

  // Returns the individual fields of a CSV row.
  static func parseCSVRow(row: String) -> [String] {
    var fields: [String] = []
    var currentField = ""
    var insideQuotes = false

    for char in row {
      switch char {
      case "\"":
        insideQuotes.toggle()
      case ",":
        if insideQuotes {
          currentField.append(char)  // Keep comma within quoted field
        } else {
          fields.append(currentField)
          currentField = ""  // Reset for the next field
        }
      default:
        currentField.append(char)
      }
    }
    // Add the last field
    fields.append(currentField)
    return fields
  }

  // Returns the field if the index is valid, empty string otherwise.
  static func getField(fields: [String], index: Int) -> String {
    return (fields.count > index) ? fields[index] : ""
  }

  // Returns an array of passwords from a CSV input.
  static func parse(input: String) -> [PasswordData]? {
    if input.isEmpty {
      return nil
    }

    var dataRows = input

    // Construct a column map.
    let firstRow = consumeCSVLine(input: &dataRows)
    if firstRow.isEmpty {
      return nil
    }

    let fields = parseCSVRow(row: firstRow)
    if fields.isEmpty {
      return nil
    }

    var fieldMap = [FieldLabel: Int]()
    var note_field_priority = 0
    for i in 0..<fields.count {
      let (priority, optionalFieldLabel) = label(forFieldName: fields[i])
      guard let fieldLabel = optionalFieldLabel else { continue }

      // Make sure the map contains the highest priority note field.
      if fieldLabel == .note {
        if priority < note_field_priority {
          continue
        } else {
          note_field_priority = priority
        }
      } else if fieldMap.keys.contains(fieldLabel) {
        // The same field appears multiple times.
        return nil
      }

      fieldMap[fieldLabel] = i
    }

    // A valid password entry must contain at least
    // an origin, a username and a password.
    guard let originIndex = fieldMap[FieldLabel.origin],
      let usernameIndex = fieldMap[FieldLabel.username],
      let passwordIndex = fieldMap[FieldLabel.password]
    else {
      return nil
    }

    // The password's note is optional.
    let noteIndex = fieldMap[FieldLabel.note]

    var passwordData = [PasswordData]()
    repeat {
      let row = seekToNextValidRow(string: &dataRows)
      if !row.isEmpty {
        let rowFields = parseCSVRow(row: row)

        var note: String? = nil
        if let noteIndex = noteIndex {
          note = getField(fields: rowFields, index: noteIndex)
        }

        passwordData.append(
          PasswordData(
            row_origin: getField(fields: rowFields, index: originIndex),
            row_username: getField(fields: rowFields, index: usernameIndex),
            row_password: getField(fields: rowFields, index: passwordIndex),
            row_note: note))
      }
    } while !dataRows.isEmpty

    return passwordData
  }
}
