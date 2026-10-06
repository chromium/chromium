# TTC Component

Cross-platform TTC code shared across Desktop, Android, and iOS.

## Structure

- **`app/public/`**: Public types (`ErrorCode`, `ToolDefinition`,
  `ToolRequest`, `ToolResponse`) accessible outside of `app/`.
- **`app/`**: Cross-platform TTC backend interface (`TtcBackend`) and Model
  Execution Service client (`TtcMesClient`).
