# Sign-in Public WebData

Freshness: 2026-09-02

## Overview
Supports maintenance and disk persistence of OAuth2 refresh tokens and token binding metadata within the WebDatabase. Conceptually situated below the rest of the Sign-in component stack, this package is strictly isolated and forbidden from depending on any other directories under `//components/signin`.

## Interface
- [`TokenWebData`](./token_web_data.h): Asynchronous public service extending `WebDataServiceBase` that schedules token operations (set, remove, retrieve) from the UI sequence to the `WebDatabase` backend sequence.
- [`TokenResult`](./token_web_data.h): Container for asynchronous `GetAllTokens` query results, containing the database execution `Result`, token-to-binding mappings, and a re-encryption flag (`should_reencrypt`).
- [`TokenServiceTable`](./token_service_table.h): SQLite `WebDatabaseTable` managing the `token_service` table, handling schema migrations, token encryption/decryption via `os_crypt_async::Encryptor`, and database queries.
- [`TokenServiceTable::TokenWithBindingInfo`](./token_service_table.h): Data structure encapsulating a decrypted OAuth2 token, its associated wrapped binding key, and mTLS token binding status flag.

## Invariants
- **Layering & Isolation**: Cannot depend on any other directories or targets under `//components/signin`.
- **Threading Model**: `TokenWebData` is invoked on the UI sequence and schedules DB operations asynchronously on the `WebDatabaseService` DB task runner. Direct `TokenServiceTable` methods execute exclusively on the background DB sequence.
- **Encryption Requirement**: All tokens written to SQLite via `TokenServiceTable::SetTokenForService` must be encrypted using `os_crypt_async::Encryptor`. If encryption fails, no token is written to storage.

## Side Effects
- **Disk Storage**: Modifies the SQLite database (`token_service` table) to persist encrypted OAuth2 tokens, wrapped binding keys, and mTLS binding flags, or delete revoked/stale tokens.
- **Metrics Logging**: Emits UMA histograms under `Signin.TokenTable.*` recording database operation outcomes, token retrieval and encryption latencies, SQL query validity, and token deletion counts.

## Verification
- **Build**: `autoninja -C out/Default components_unittests`
- **Test**: `out/Default/components_unittests --gtest_filter="TokenServiceTableTest.*"`
