self.importScripts('websocket_connection.js');
self.addEventListener('message', (e) => {
  connectWebSocketWithMessageCallback(e.data.url, self.postMessage);
});
