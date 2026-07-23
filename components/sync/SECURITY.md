# Security Model for //components/sync

Chromium Sync synchronizes user data (e.g., bookmarks, passwords, history) across multiple devices under the same account. It runs entirely in the browser process.

## Threat Model

### Trust Assumptions

1. **Other Syncing Devices are Trusted:**
   Other devices logged into the same account are fully trusted. A remote device can add, modify, or delete local user data (including passwords) without user confirmation. This is working as intended. However, the client must still handle invalid or malformed data, and must not crash or CHECK-fail. (Such "invalid data" can happen e.g. when data from an older or a newer client is received.)
2. **The Sync Server is Trusted:**
   The server is assumed to not be malicious. It *mostly* acts as a transport layer, moving data between different Chrome clients belonging to the same user, but some data may also originate from the server.
   * **Encryption (Nigori):** Most data types can optionally be encrypted with a custom passphrase. In this case, the server must not be able to decrypt the data or coerce the client to downgrade encryption.
3. **Local Storage & Network:**
   Sync assumes a secure local environment (profile directory) and a secure network transport (HTTPS).

### In-Scope Vulnerabilities

* **Bypassing Encryption:** Decrypting encrypted data without the user's passphrase or trusted vault keys.
* **Key/Passphrase Leakage:** Leaking Nigori keys, passphrases, or trusted vault keys (e.g., in logs, crash dumps, or unencrypted IPCs).
* **Remote Code Execution (RCE):** Memory corruption or execution of arbitrary code via malformed protobufs sent by the server.
* **Auth Token Exposure:** Exposing Gaia OAuth2 tokens to unauthorized processes or web content.

### Out of Scope / Not a Security Boundary

* **Remote Writes:** Modifying local data from a sibling device (e.g., injecting a bookmark or password).
* **Compromised Server Modifying Data:** A compromised server injecting/modifying data is an account/backend compromise, not a client-side vulnerability (as long as it doesn't cause crashes/RCE).
* **Direct File Access:** Attacker with local access to the profile directory reading/modifying local sync databases (e.g., `DataTypeStore`).
