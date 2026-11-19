# Fake DMServer Administration REST API

`fake_dmserver` exposes a local REST API under `/admin/api/` on port 6112.
Use this API to automate policy testing, inspect client state, and issue
remote commands.

## Base URL

```text
http://127.0.0.1:6112/admin/api
```

## Security Requirements

All `POST` endpoints restrict cross-origin access:
* The request MUST include the header `Content-Type: application/json`.
  Other requests return HTTP 415 Unsupported Media Type.
* If an `Origin` header is present, it MUST resolve to localhost. Requests from
  non-localhost origins return HTTP 403 Forbidden.

## Error Format

Failed requests return an HTTP 4xx status with a JSON body:

```json
{"error": "<message>"}
```

## Endpoints

### Read-only endpoints

| Method | Path | JSON Body |
| --- | --- | --- |
| `GET` | `/admin/api/clients` | `{"clients": [...]}` |
| `GET` | `/admin/api/reports/browser` | `{"reports": [...]}` |
| `GET` | `/admin/api/reports/profile` | `{"reports": [...]}` |
| `GET` | `/admin/api/reports/events` | `{"events": [...]}` |
| `GET` | `/admin/api/commands` | `{"commands": [...]}` |
| `GET` | `/admin/api/policies` | Raw `policy.json` object |

All list endpoints return a JSON object with a single array field:
* `/admin/api/clients`: Registered clients and their DM tokens.
* `/admin/api/reports/browser`: Browser reports uploaded by clients.
* `/admin/api/reports/profile`: Profile reports uploaded by clients.
* `/admin/api/reports/events`: Real-time reporting events received.
* `/admin/api/commands`: Issued commands, their status, and results. Command
  status is `PENDING`, `FETCHED`, or `RESULT_AVAILABLE`. `command_id` and
  `timestamp_ms` are numbers.
* `/admin/api/policies`: Current `policy.json` contents (or `{}`).

Example request:
```bash
curl http://127.0.0.1:6112/admin/api/clients
```

Example response:
```json
{
  "clients": [
    {
      "device_id": "device-123",
      "device_token": "token-abc",
      "machine_name": "workstation",
      "username": "alice@example.com"
    }
  ]
}
```

### Remote Commands

#### `POST /admin/api/commands`

Queues a remote command for execution. The server sends an FCM push
notification to connected clients immediately after queuing the command.

Request body fields:
* `scope`: (String) Target scope. Must be `"browser"` or `"profile"`.
* `command_type`: (Integer) Any valid `RemoteCommand.Type` enum value from
  `components/policy/proto/device_management_backend.proto`.
* `payload`: (String) Serialized JSON payload string (e.g. `"{}"`).

Common desktop command types:
* `12`: `BROWSER_CLEAR_BROWSING_DATA`
* `14`: `BROWSER_ROTATE_ATTESTATION_CREDENTIAL`
* `18`: `BROWSER_EXTENSION_UPDATE_CHECK`

Returns HTTP 200 with:
```json
{"success": true, "command_id": 1}
```

Example request:
```bash
curl -X POST http://127.0.0.1:6112/admin/api/commands \
  -H "Content-Type: application/json" \
  -d '{"scope": "browser", "command_type": 12, "payload": "{}"}'
```

### Policies

#### `GET /admin/api/policies`

Returns the raw `policy.json` file contents (`application/json`), or `{}` if
the file cannot be read.

Example request:
```bash
curl http://127.0.0.1:6112/admin/api/policies
```

#### `POST /admin/api/policies`

Validates and writes the JSON request body to `policy.json`, then broadcasts a
policy invalidation to connected FCM clients.

Returns HTTP 200 with:
```json
{"success": true}
```

Returns HTTP 400 with `{"error": "Invalid policies"}` if the request body is
not valid JSON or fails policy validation.

Example request:
```bash
curl -X POST http://127.0.0.1:6112/admin/api/policies \
  -H "Content-Type: application/json" \
  -d '{"policy_user": "alice@example.com", "machine": {"ShowHomeButton": true}}'
```

#### `POST /admin/api/policies/invalidate`

Reloads `policy.json` and broadcasts a policy invalidation to connected FCM
clients.

Returns HTTP 200 with:
```json
{"success": true, "sent": true}
```

`sent` is `true` if at least one FCM client is connected, or `false` otherwise.
Returns HTTP 400 with `{"error": "Invalid policy file"}` if `policy.json` is
invalid.

Example request:
```bash
curl -X POST http://127.0.0.1:6112/admin/api/policies/invalidate \
  -H "Content-Type: application/json"
```
